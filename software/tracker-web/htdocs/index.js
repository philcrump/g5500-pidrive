'use strict';

const ws_url = window.location.protocol == "https:" ? "wss:" : "ws:" + "//" + window.location.hostname + ":" + window.location.port + "/";

let data_status = {};

let domviews = [];

domviews.push({
  'dom': 'demand-status-ui',
  'view':
  {
    view: function()
    {
      if(!data_status.hasOwnProperty('demand'))
      {
        return;
      }

      return [
        m("p", [
          m("label", 'Azimuth:'),
          m("span", ` ${decimalFix(data_status['demand'].az_deg,1)}°`)
        ]),
        m("p", [
          m("label", 'Elevation:'),
          m("span", ` ${decimalFix(data_status['demand'].el_deg,1)}°`)
        ])
      ];
    }
  }
});

domviews.push({
  'dom': 'encoder-status-ui',
  'view': 
  {
    view: function()
    {
      if(!data_status.hasOwnProperty('encoder'))
      {
        return;
      }

      return [
        m("p", [
          m("label", 'Azimuth:'),
          m("span", ` ${decimalFix(data_status['encoder'].az_deg,1)}°`)
        ]),
        m("p", [
          m("label", 'Elevation:'),
          m("span", ` ${decimalFix(data_status['encoder'].el_deg,1)}°`)
        ])
      ];
    }
  }
});

domviews.push({
  'dom': 'heading-status-ui',
  'view':
  {
    view: function()
    {
      if(!data_status.hasOwnProperty('mag') || !data_status.hasOwnProperty('heading') || !data_status.hasOwnProperty('headingoffset'))
      {
        return;
      }

      let heading = Math.atan2(data_status['mag'].y, data_status['mag'].x) * (180.0/Math.PI);
      while(heading < 0) { heading += 360.0; }
      while(heading >= 360.0) { heading -= 360.0; }

      return [
        m("p", [
          m("label", 'Magnetometer (unfiltered):'),
          m("span", ` ${decimalFix(data_status['mag'].x,0)}, ${decimalFix(data_status['mag'].y,0)} (${decimalFix(heading,0)}°)`)
        ]),
        m("p", [
          m("label", 'Calibration Progress:'),
          m("span", `${decimalFix(data_status['heading'].calstatus,0)}: ${decimalFix(data_status['heading'].calstatus_angularcounter,0)} / ${decimalFix(data_status['heading'].calstatus_angularpoints,0)}`)
        ]),
        m("p", [
          m("label", 'Heading Offset:'),
          (
            data_status['headingoffset'].valid ?
              m("span", ` ${decimalFix(data_status['headingoffset'].deg,1)}° (σ = ${decimalFix(data_status['headingoffset'].stddev_deg,1)}°)`)
            :
              m("span", ` Invalid`)
          )
        ])
      ];
    }
  }
});

domviews.push({
  'dom': 'gpsd-status-ui',
  'view': 
  {
    view: function()
    {
      if(!data_status.hasOwnProperty('gpsd'))
      {
        return;
      }

      return [
        m("p", [
          m("label", 'GPS Fix:'),
          m("span", ` ${decimalFix(data_status['gpsd'].gpsd_fixmode,0)}D`)
        ]),
        m("p", [
          m("label", 'Latitude:'),
          m("span", ` ${decimalFix(data_status['gpsd'].gpsd_latitude,4)}°`)
        ]),
        m("p", [
          m("label", 'Longitude:'),
          m("span", ` ${decimalFix(data_status['gpsd'].gpsd_longitude,4)}°`)
        ]),
        m("p", [
          m("label", 'Satellites:'),
          m("span", ` ${decimalFix(data_status['gpsd'].gpsd_sats,0)}`)
        ])
      ];
    }
  }
});

domviews.push({
  'dom': 'sun-status-ui',
  'view': 
  {
    view: function()
    {
      if(!data_status.hasOwnProperty('sun'))
      {
        return;
      }

      return [
        m("p", [
          m("label", 'Azimuth:'),
          m("span", ` ${decimalFix(data_status['sun'].az_deg,1)}°`)
        ]),
        m("p", [
          m("label", 'Elevation:'),
          m("span", ` ${decimalFix(data_status['sun'].el_deg,1)}°`)
        ])
      ];
    }
  }
});

domviews.push({
  'dom': 'iss-status-ui',
  'view': 
  {
    view: function()
    {
      if(!data_status.hasOwnProperty('iss'))
      {
        return;
      }

      return [
        m("p", [
          m("label", 'Azimuth:'),
          m("span", ` ${decimalFix(data_status['iss'].az_deg,1)}°`)
        ]),
        m("p", [
          m("label", 'Elevation:'),
          m("span", ` ${decimalFix(data_status['iss'].el_deg,1)}°`)
        ])
      ];
    }
  }
});

domviews.push({
  'dom': 'voltages-status-ui',
  'view': 
  {
    view: function()
    {
      if(!data_status.hasOwnProperty('voltages'))
      {
        return;
      }

      return [
        m("p", [
          m("label", 'PoE:'),
          m("span", ` ${decimalFix(data_status['voltages'].voltages_poe,2)} V`)
        ]),
        m("p", [
          m("label", 'PoE Current:'),
          m("span", ` ${decimalFix(data_status['voltages'].voltages_ichrg,2)} A`)
        ]),
        m("p", [
          m("label", 'Battery:'),
          m("span", ` ${decimalFix(data_status['voltages'].voltages_batt,2)} V`)
        ]),
        m("p", [
          m("label", 'High Bus:'),
          m("span", ` ${decimalFix(data_status['voltages'].voltages_high,2)} V`)
        ])
      ];
    }
  }
});

let ws_monitor = null;
let ws_monitor_buffer = [];

let ws_control = null;
let ws_control_buffer = [];

let render_busy = false;
const render_interval = 10;
let render_timer = setInterval(() =>
{
  if(!render_busy)
  {
    render_busy = true;
    while(ws_monitor_buffer.length > 0)
    {
      /* Pull newest data off the buffer and render it */
      let data_frame = JSON.parse(ws_monitor_buffer.pop());

      data_status = data_frame;
    }
    m.redraw();
    render_busy = false;
  }
  else
  {
    console.log("Slow render blocking next frame, configured interval is ", render_interval);
  }
}, render_interval);

document.addEventListener("DOMContentLoaded", (event) =>
{ 
  ws_monitor = new strWebsocket(ws_url, "monitor", ws_monitor_buffer);
  ws_control = new strWebsocket(ws_url, "control", ws_control_buffer);

  domviews.forEach(domview => {
      m.mount(document.getElementById(domview.dom), domview.view);
  });

  document.getElementById('control-azel-submit').onclick = (e) =>
  {
    const _azimuth = document.getElementById('control-az-input').value;
    const _elevation = document.getElementById('control-el-input').value;
    ws_control.sendMessage("M"+_azimuth+","+_elevation);
  }

  document.getElementById('control-calibrate-submit').onclick = (e) =>
  {
    ws_control.sendMessage("CAL");
  }

  document.getElementById('control-sun-submit').onclick = (e) =>
  {
    ws_control.sendMessage("SUN");
  }
  document.getElementById('control-iss-submit').onclick = (e) =>
  {
    ws_control.sendMessage("ISS");
  }

  const doc_modal_ui = document.getElementById('disconnectionSet');

  setInterval(function() {
    m.redraw();
    if(data_status.timestamp < ((new Date()).getTime() - 3000))
    {
      if(doc_modal_ui.style.display != "block")
      {
        doc_modal_ui.style.display = "block";
        ws_monitor.close();
        ws_control.close();
      }
    }
    else
    {
      if(doc_modal_ui.style.display != "none")
      {
        doc_modal_ui.style.display = "none";
      }
    }
  }, 250);
});

// ############################################################

function decimalFix(num, decimals, leading=0, leadingChar='0')
{
  const t = Math.pow(10, decimals);
  return ((Math.round((num * t) + (decimals>0?1:0)*(Math.sign(num) * (10 / Math.pow(100, decimals)))) / t).toFixed(decimals)).toString().padStart(leading+1+decimals, leadingChar);
}
