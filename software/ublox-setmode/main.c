#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

bool verbose = false;

const int32_t gnss_aid_position_latitude    = (51.64 / 1e-7); // mid-south-England
const int32_t gnss_aid_position_longitude   = (-1.48 / 1e-7);
const int32_t gnss_aid_position_altitude    =    (10 / 1e-2); // 10m
const int32_t gnss_aid_position_stddev      =   (199 / 1e-5); // 199km

uint8_t gnss_hard_reset[] =
    {   0xB5, 0x62,
        0x06, 0x04, // UBX-CFG-RST
        0x04, 0x00, // Length
        0xff, 0xff, // navBbrMask (0 = hot, 1 = warm, ffff = cold)
        0x04, // resetMode (0 = hardware, 1 = software, 2 = gnss, 4 = shutdown then hardware)
        0x00, // Reserved
        0x00, 0x00 // Checksum
    };

uint8_t gnss_aid_position[] =
    {   0xB5, 0x62,
        0x13, 0x40, // UBX-MGA-INI
        0x14, 0x00, // Length
        0x01, // Type: UBX-MGA-INI-POS_LLH
        0x00, // Version: 0
        0x00, 0x00, // Reserved
        0x00, 0x00, 0x00, 0x00, // Latitude 1e-7
        0x00, 0x00, 0x00, 0x00, // Longitude 1e-7
        0x00, 0x00, 0x00, 0x00, // Altitude cm
        0x00, 0x00, 0x00, 0x00, // Std Dev cm
        0x00, 0x00 // Checksum
    };

uint8_t set_navmodel_stationary[] =
    {   0xB5, 0x62,
        0x06, 0x24, // UBX-CFG-NAV5
        0x24, 0x00, // Length 
        0xFF, 0xFF, // Bitmask
        0x02, // Model (Stationary = 0x02, airborne = 0x06)
        0x03, // Fix Type (Auto 2D/3D)
        0x00, 0x00, 0x00, 0x00, // 2D Altitude Value
        0x10, 0x27, 0x00, 0x00, // 2D Altitude Variance
        0x05, // Minimum GNSS Satellite Elevation
        0x00, // Reserved
        0xFA, 0x00, // Position DOP Mask
        0xFA, 0x00, // Time DOP Mask
        0x64, 0x00, // Position Accuracy Mask
        0x2C, 0x01, // Time Accuracy Mask
        0x00, // Static hold threshold
        0x00, // DGNSS Timeout
        0x00, // Min Satellites for Fix
        0x00, // Min C/N0 Threshold for Satellites
        0x10, 0x27, // Reserved
        0x00, 0x00, // Static Hold Distance Threshold
        0x00, // UTC Standard (Automatic)
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00 // Checksum
    };

uint8_t set_sbas_egnos[] =
    {   0xB5, 0x62,
        0x06, 0x16, // UBX-CFG-SBAS
        0x08, 0x00, // Length
        0x01, // Mode (enabled)
        0x07, // Usage (range, corrections, integrity)
        0x03, // SBAS Channels (3, field obselete)
        0x00, 0x51, 0x08, 0x00, 0x00, // PRN Bitmask (EGNOS-only)
        0x00, 0x00 // Checksum
    };

void usage( void )
{
    printf("Usage: ubloxSetMode -d <device>\n");  
}
 
unsigned char ReadBByteFromSerial(int fd, int *k)
{
    unsigned char tmp;
   
    (*k)++;
     
    if (read(fd, &tmp, 1) > 0)
    {
        // if (tmp & 0x80)
        // {
            // printf("<%2x> ", tmp);
        // }
        // else
        // {
            // printf("%c", tmp);
        // }
    }
   
    return tmp;
}

void calcChecksum(uint8_t *command, int32_t command_length)
{
    uint8_t ck_a = 0, ck_b = 0;
    for(int32_t i = 2; i < (command_length-2); i++)
    {
        ck_a += command[i];
        ck_b += ck_a;
    }
    command[command_length-2] = ck_a;
    command[command_length-1] = ck_b;

    //printf("ck: 0x%02x, 0x%02x\n", ck_a, ck_b);
}

bool setMode(int fd, uint8_t *command, int32_t command_length)
{
    int Retries, k;
    bool result;

    Retries = 0;
    k = 0;
    result = false;
   
    while ((Retries < 5) && !result)
    {
        calcChecksum(command, command_length);

        if(verbose) printf(">> Sending Command (Attempt %d of 5) >>\n", (Retries+1));
        if(write(fd, command, command_length) != command_length)
        {
            Retries++;
            continue;
        }
       
        Retries++;
        k = 0;
       
        if(verbose) printf("<< Receiving ACK <<\n");
        while ((k < 1000) && !result)
        {
            /* Check for ACK */
            if (ReadBByteFromSerial(fd, &k) == 0xb5)
            {
                if (ReadBByteFromSerial(fd, &k) == 0x62)
                {
                    if (ReadBByteFromSerial(fd, &k) == 0x05)
                    {
                        if (ReadBByteFromSerial(fd, &k) == 0x01)
                        {
                            ReadBByteFromSerial(fd, &k);
                            ReadBByteFromSerial(fd, &k);

                            /* Check ACK is for correct message ID */
                            if (ReadBByteFromSerial(fd, &k) == command[2])
                            {
                                if (ReadBByteFromSerial(fd, &k) == command[3])
                                {
                                    if(verbose) printf("|| Received ACK OK!\n");
                                    result = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}
 
int main(int argc, char *argv[])
{   
    bool result;
    int fd;
    int option = 0;
    char devName[50];
    struct termios options;
   
    memset(devName, 0, sizeof(devName));
    while((option = getopt( argc, argv, "vd:")) != -1)
    {
        switch(option)
        {
            case 'd':
                printf("Configuring serial device: %s\n", optarg);
                strncpy(devName, optarg, sizeof(devName)-1);                
                break;
            case 'v':
                printf(" * Verbose Mode Enabled\n");
                verbose = true;
                break;
            default:
                usage();
                return 0;
        }        
    }
 
    if(devName[0] == 0)
    {
        usage();
        return 1;
    }
       
    /* Open Serial Port */
    fd = open(devName, O_RDWR);
    if(fd < 0)
    {
        printf("Error: Cannot open serial device '%s'\n", devName);
        return 1;
    }
     
    /* Configure Serial Port */
    tcgetattr(fd, &options);
 
    options.c_lflag &= ~ECHO;
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 10;
 
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_oflag &= ~ONLCR;
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~IXON;
    options.c_iflag &= ~IXOFF;
 
    tcsetattr(fd, TCSANOW, &options);

    memcpy(&gnss_aid_position[8], &gnss_aid_position_latitude, sizeof(int32_t));
    memcpy(&gnss_aid_position[12], &gnss_aid_position_longitude, sizeof(int32_t));
    memcpy(&gnss_aid_position[16], &gnss_aid_position_altitude, sizeof(int32_t));
    memcpy(&gnss_aid_position[20], &gnss_aid_position_stddev, sizeof(int32_t));
    calcChecksum(gnss_aid_position, sizeof(gnss_aid_position));

    printf("Configuring Position Aid..\n");
    result = setMode(fd, gnss_aid_position, sizeof(gnss_aid_position));
    if(!result)
    {
        printf("** FAILED to configure Position Aid **\n");
        close(fd);
        return 1;
    }

    printf("Configuring Navigation Model..\n");
    result = setMode(fd, set_navmodel_stationary, sizeof(set_navmodel_stationary));
    if(!result)
    {
        printf("** FAILED to configure Navigation Model **\n");
        close(fd);
        return 1;
    }
 
    printf("Configuring SBAS Mode for EGNOS..\n");
    result = setMode(fd, set_sbas_egnos, sizeof(set_sbas_egnos));
    if(!result)
    {
        printf("** FAILED to configure SBAS Mode **\n");
        close(fd);
        return 1;
    }

    close(fd);

    printf("Device Configuration Successful\n");
   
    return 0;
}