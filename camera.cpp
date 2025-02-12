#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"
#include "esp32-hal-ledc.h"


#define on_board_red_LED 33
#define LED_FLASH_PIN 4

// Servos configuration
#define SERVO_FREQUENCY  50  // 50HZ
#define SERVO_RESOLUTION 16 // 16 bits
#define SERVO_TIMER_WIDTH_TICKS 65536 // 2**SERVO_RESOLUTION)

#define SERVO_uS_LOW  1000      // 1000us
#define SERVO_uS_HIGH 2000      // 2000us

#define SERVO_PIN_H 12    
#define SERVO_PIN_V 13     


// WIFI credentials
const char* ssid = "WIFICOTEAU";
const char* password = "kitesurf9397";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!doctype html>
<html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width,initial-scale=1">
        <title>Camera</title>
        <style>
            body {
                font-family: Arial,Helvetica,sans-serif;
                background: #181818;
                color: #EFEFEF;
                font-size: 16px
            }

            section.main {
                display: flex
            }

            #menu,section.main {
                flex-direction: column
            }

            #menu {
                display: none;
                flex-wrap: nowrap;
                min-width: 340px;
                background: #363636;
                padding: 8px;
                border-radius: 4px;
                margin-top: -10px;
                margin-right: 10px;
            }

            #content {
                display: flex;
                flex-wrap: wrap;
                align-items: stretch
            }
            
            a#save-capture {
                display: block;
                margin: 2px;
                padding: 1px 4px 2px 4px;
                border: 0;
                line-height: 20px;
                cursor: pointer;
                color: #fff;
                background: #ff3034;
                border-radius: 5px;
                font-size: 16px;
                outline: 0;
                text-decoration: none;
                width: 100px
           }

            figure {
                padding: 0px;
                margin: 0;
                -webkit-margin-before: 0;
                margin-block-start: 0;
                -webkit-margin-after: 0;
                margin-block-end: 0;
                -webkit-margin-start: 0;
                margin-inline-start: 0;
                -webkit-margin-end: 0;
                margin-inline-end: 0
            }

            figure img {
                display: block;
                width: 100%;
                height: auto;
                border-radius: 4px;
                margin-top: 8px;
            }

            @media (min-width: 800px) and (orientation:landscape) {
                #content {
                    display:flex;
                    flex-wrap: nowrap;
                    align-items: stretch
                }

                figure img {
                    display: block;
                    max-width: 100%;
                    max-height: calc(100vh - 40px);
                    width: auto;
                    height: auto
                }

                figure {
                    padding: 0 0 0 0px;
                    margin: 0;
                    -webkit-margin-before: 0;
                    margin-block-start: 0;
                    -webkit-margin-after: 0;
                    margin-block-end: 0;
                    -webkit-margin-start: 0;
                    margin-inline-start: 0;
                    -webkit-margin-end: 0;
                    margin-inline-end: 0
                }
            }

            section#buttons {
                display: flex;
                flex-wrap: nowrap;
                justify-content: space-between
            }

            #nav-toggle {
                cursor: pointer;
                display: block
            }

            #nav-toggle-cb {
                outline: 0;
                opacity: 0;
                width: 0;
                height: 0
            }

            #nav-toggle-cb:checked+#menu {
                display: flex
            }
            
            .input-grouptop {
                display: flex;
                flex-wrap: nowrap;
                line-height: 22px;
                margin: 5px 0
                width: 100px;
            }

            .input-grouptop>label {
                display: inline-block;
                padding-right: 10px;
                width: 100px;
            }

            .input-grouptop input,.input-group select {
                flex-grow: 1
                width: 200px;
            }
            .input-group {
                display: flex;
                flex-wrap: nowrap;
                line-height: 22px;
                margin: 5px 0
            }

            .input-group>label {
                display: inline-block;
                padding-right: 10px;
                min-width: 47%
            }

            .input-group input,.input-group select {
                flex-grow: 1
            }

            .range-max,.range-min {
                display: inline-block;
                padding: 0 5px
                width: 100px;
            }

            button, .button {
                display: block;
                margin: 5px;
                padding: 0 12px;
                border: 0;
                line-height: 28px;
                cursor: pointer;
                color: #fff;
                background: #ff3034;
                border-radius: 5px;
                font-size: 16px;
                outline: 0
            }


            button:hover {
                background: #ff494d
            }

            button:active {
                background: #f21c21
            }

            button.disabled {
                cursor: default;
                background: #a0a0a0
            }

            input[type=range] {
                -webkit-appearance: none;
                width: 200px;
                height: 22px;
                background: #363636;
                cursor: pointer;
                margin: 0
            }

            input[type=range]:focus {
                outline: 0
            }

            input[type=range]::-webkit-slider-runnable-track {
                width: 100%;
                height: 2px;
                cursor: pointer;
                background: #EFEFEF;
                border-radius: 0;
                border: 0 solid #EFEFEF
            }

            input[type=range]::-webkit-slider-thumb {
                border: 1px solid rgba(0,0,30,0);
                height: 22px;
                width: 22px;
                border-radius: 50px;
                background: #ff3034;
                cursor: pointer;
                -webkit-appearance: none;
                margin-top: -11.5px
            }

            input[type=range]:focus::-webkit-slider-runnable-track {
                background: #EFEFEF
            }

            input[type=range]::-moz-range-track {
                width: 100%;
                height: 2px;
                cursor: pointer;
                background: #EFEFEF;
                border-radius: 0;
                border: 0 solid #EFEFEF
            }

            input[type=range]::-moz-range-thumb {
                border: 1px solid rgba(0,0,30,0);
                height: 22px;
                width: 22px;
                border-radius: 50px;
                background: #ff3034;
                cursor: pointer
            }

            input[type=range]::-ms-track {
                width: 100%;
                height: 2px;
                cursor: pointer;
                background: 0 0;
                border-color: transparent;
                color: transparent
            }

            input[type=range]::-ms-fill-lower {
                background: #EFEFEF;
                border: 0 solid #EFEFEF;
                border-radius: 0
            }

            input[type=range]::-ms-fill-upper {
                background: #EFEFEF;
                border: 0 solid #EFEFEF;
                border-radius: 0
            }

            input[type=range]::-ms-thumb {
                border: 1px solid rgba(0,0,30,0);
                height: 22px;
                width: 22px;
                border-radius: 50px;
                background: #ff3034;
                cursor: pointer;
                height: 2px
            }

            input[type=range]:focus::-ms-fill-lower {
                background: #EFEFEF
                height: 22px;
                width: 22px;
            }

            input[type=range]:focus::-ms-fill-upper {
                background: #363636
                height: 22px;
                width: 22px;
            }

            .switch {
                display: block;
                position: relative;
                line-height: 22px;
                font-size: 16px;
                height: 22px
            }

            .switch input {
                outline: 0;
                opacity: 0;
                width: 0;
                height: 0
            }

            .slider {
                width: 50px;
                height: 22px;
                border-radius: 22px;
                cursor: pointer;
                background-color: grey
            }

            .slider,.slider:before {
                display: inline-block;
                transition: .4s
            }

            .slider:before {
                position: relative;
                content: "";
                border-radius: 50%;
                height: 16px;
                width: 16px;
                left: 4px;
                top: 3px;
                background-color: #fff
            }

            input:checked+.slider {
                background-color: #ff3034
            }

            input:checked+.slider:before {
                -webkit-transform: translateX(26px);
                transform: translateX(26px)
            }

            select {
                border: 1px solid #363636;
                font-size: 14px;
                height: 22px;
                outline: 0;
                border-radius: 5px
            }

            .image-container {
                position: relative;
                min-width: 160px
            }

            .close {
                position: absolute;
                right: 5px;
                top: 5px;
                background: #ff3034;
                width: 16px;
                height: 16px;
                border-radius: 100px;
                color: #fff;
                text-align: center;
                line-height: 18px;
                cursor: pointer
            }

            .hidden {
                display: none
            }

            input[type=text] {
                border: 1px solid #363636;
                font-size: 14px;
                height: 20px;
                margin: 1px;
                outline: 0;
                border-radius: 5px
            }

            .inline-button {
                line-height: 20px;
                margin: 2px;
                padding: 1px 4px 2px 4px;
            }

            label.toggle-section-label {
                cursor: pointer;
                display: block
            }

            input.toggle-section-button {
                outline: 0;
                opacity: 0;
                width: 0;
                height: 0
            }

            input.toggle-section-button:checked+section.toggle-section {
                display: none
            }

        </style>
    </head>
    <body>
    <div class="input-grouptop" id="horizontal-group">
       <label for="horizontal">Horizontal</label>
       <div class="range-min">-90</div>
       <input type="range" id="horizontal" min="-90" max="90"  step="20"value="0" class="default-action">
       <div class="range-max">+90</div>
    </div> 
    <div class="input-grouptop" id="vertical-group">
       <label for="vertical">Vertical</label>
       <div class="range-min">-90</div>
       <input type="range" id="vertical" min="-90" max="90" step="20" value="0" class="default-action">
       <div class="range-max">+90</div>
    </div>    
    
    <div id="buttons">
      <button class="inline-button" id="capture">Capture</button>
      <button class="inline-button" id="toggle-stream">Start Stream</button>
      <button class="inline-button" id="toggle-flash">Flash On</button>
    </div>
    
    <figure>
      <div id="stream-container" class="image-container hidden">
        <a id="save-capture" href="#" class="inline-button" download="capture.jpg">Download</a>
        <img id="stream" src="" crossorigin>
      </div>
    </figure> 
       
    <label for="nav-toggle-cb0" class="toggle-section-label">&#9776;&nbsp;&nbsp;Camera settings</label>
    <input type="checkbox" id="nav-toggle-cb0" class="hidden toggle-section-button" checked="checked">
    <section class="toggle-section">
      <div id="content">
        <div id="sidebar">
                    <input type="checkbox" id="nav-toggle-cb" checked="checked">
                    <nav id="menu">

                        <section id="xclk-section" class="nothidden">
                            <div class="input-group" id="set-xclk-group">
                                <label for="set-xclk">XCLK MHz</label>
                                <div class="text">
                                    <input id="xclk" type="text" minlength="1" maxlength="2" size="2" value="20">
                                </div>
                                <button class="inline-button" id="set-xclk">Set</button>
                            </div>
                        </section>

                        <div class="input-group" id="framesize-group">
                            <label for="framesize">Resolution</label>
                            <select id="framesize" class="default-action">
                                <!-- 2MP -->
                                <option value="15">UXGA(1600x1200)</option>
                                <option value="14">SXGA(1280x1024)</option>
                                <option value="13">HD(1280x720)</option>
                                <option value="12">XGA(1024x768)</option>
                                <option value="11">SVGA(800x600)</option>
                                <option value="10">VGA(640x480)</option>
                                <option value="9">HVGA(480x320)</option>
                                <option value="8">CIF(400x296)</option>
                                <!--option value="7">320x320</option--> <!-- Unsupported on ov2640 -->
                                <option value="6">QVGA(320x240)</option>
                                <option value="5">240x240</option>
                                <option value="4">HQVGA(240x176)</option>
                                <option value="3">QCIF(176x144)</option>
                                <option value="2">128x128</option>
                                <option value="1">QQVGA(160x120)</option>
                                <option value="0">96x96</option>
                            </select>
                        </div>
                        <div class="input-group" id="quality-group">
                            <label for="quality">Quality</label>
                            <div class="range-min">4</div>
                            <input type="range" id="quality" min="4" max="63" value="10" class="default-action">
                            <div class="range-max">63</div>
                        </div>
                        <div class="input-group" id="brightness-group">
                            <label for="brightness">Brightness</label>
                            <div class="range-min">-2</div>
                            <input type="range" id="brightness" min="-2" max="2" value="0" class="default-action">
                            <div class="range-max">2</div>
                        </div>
                        <div class="input-group" id="contrast-group">
                            <label for="contrast">Contrast</label>
                            <div class="range-min">-2</div>
                            <input type="range" id="contrast" min="-2" max="2" value="0" class="default-action">
                            <div class="range-max">2</div>
                        </div>
                        <div class="input-group" id="saturation-group">
                            <label for="saturation">Saturation</label>
                            <div class="range-min">-2</div>
                            <input type="range" id="saturation" min="-2" max="2" value="0" class="default-action">
                            <div class="range-max">2</div>
                        </div>
                        <div class="input-group" id="special_effect-group">
                            <label for="special_effect">Special Effect</label>
                            <select id="special_effect" class="default-action">
                                <option value="0" selected="selected">No Effect</option>
                                <option value="1">Negative</option>
                                <option value="2">Grayscale</option>
                                <option value="3">Red Tint</option>
                                <option value="4">Green Tint</option>
                                <option value="5">Blue Tint</option>
                                <option value="6">Sepia</option>
                            </select>
                        </div>
                        <div class="input-group" id="awb-group">
                            <label for="awb">AWB</label>
                            <div class="switch">
                                <input id="awb" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="awb"></label>
                            </div>
                        </div>
                        <div class="input-group" id="awb_gain-group">
                            <label for="awb_gain">AWB Gain</label>
                            <div class="switch">
                                <input id="awb_gain" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="awb_gain"></label>
                            </div>
                        </div>
                        <div class="input-group" id="wb_mode-group">
                            <label for="wb_mode">WB Mode</label>
                            <select id="wb_mode" class="default-action">
                                <option value="0" selected="selected">Auto</option>
                                <option value="1">Sunny</option>
                                <option value="2">Cloudy</option>
                                <option value="3">Office</option>
                                <option value="4">Home</option>
                            </select>
                        </div>
                        <div class="input-group" id="aec-group">
                            <label for="aec">AEC SENSOR</label>
                            <div class="switch">
                                <input id="aec" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="aec"></label>
                            </div>
                        </div>
                        <div class="input-group" id="aec2-group">
                            <label for="aec2">AEC DSP</label>
                            <div class="switch">
                                <input id="aec2" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="aec2"></label>
                            </div>
                        </div>
                        <div class="input-group" id="ae_level-group">
                            <label for="ae_level">AE Level</label>
                            <div class="range-min">-2</div>
                            <input type="range" id="ae_level" min="-2" max="2" value="0" class="default-action">
                            <div class="range-max">2</div>
                        </div>
                        <div class="input-group" id="aec_value-group">
                            <label for="aec_value">Exposure</label>
                            <div class="range-min">0</div>
                            <input type="range" id="aec_value" min="0" max="1200" value="204" class="default-action">
                            <div class="range-max">1200</div>
                        </div>
                        <div class="input-group" id="agc-group">
                            <label for="agc">AGC</label>
                            <div class="switch">
                                <input id="agc" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="agc"></label>
                            </div>
                        </div>
                        <div class="input-group hidden" id="agc_gain-group">
                            <label for="agc_gain">Gain</label>
                            <div class="range-min">1x</div>
                            <input type="range" id="agc_gain" min="0" max="30" value="5" class="default-action">
                            <div class="range-max">31x</div>
                        </div>
                        <div class="input-group" id="gainceiling-group">
                            <label for="gainceiling">Gain Ceiling</label>
                            <div class="range-min">2x</div>
                            <input type="range" id="gainceiling" min="0" max="6" value="0" class="default-action">
                            <div class="range-max">128x</div>
                        </div>
                        <div class="input-group" id="bpc-group">
                            <label for="bpc">BPC</label>
                            <div class="switch">
                                <input id="bpc" type="checkbox" class="default-action">
                                <label class="slider" for="bpc"></label>
                            </div>
                        </div>
                        <div class="input-group" id="wpc-group">
                            <label for="wpc">WPC</label>
                            <div class="switch">
                                <input id="wpc" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="wpc"></label>
                            </div>
                        </div>
                        <div class="input-group" id="raw_gma-group">
                            <label for="raw_gma">Raw GMA</label>
                            <div class="switch">
                                <input id="raw_gma" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="raw_gma"></label>
                            </div>
                        </div>
                        <div class="input-group" id="lenc-group">
                            <label for="lenc">Lens Correction</label>
                            <div class="switch">
                                <input id="lenc" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="lenc"></label>
                            </div>
                        </div>
                        <div class="input-group" id="hmirror-group">
                            <label for="hmirror">H-Mirror</label>
                            <div class="switch">
                                <input id="hmirror" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="hmirror"></label>
                            </div>
                        </div>
                        <div class="input-group" id="vflip-group">
                            <label for="vflip">V-Flip</label>
                            <div class="switch">
                                <input id="vflip" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="vflip"></label>
                            </div>
                        </div>
                        <div class="input-group" id="dcw-group">
                            <label for="dcw">DCW (Downsize EN)</label>
                            <div class="switch">
                                <input id="dcw" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="dcw"></label>
                            </div>
                        </div>
                        <div class="input-group" id="colorbar-group">
                            <label for="colorbar">Color Bar</label>
                            <div class="switch">
                                <input id="colorbar" type="checkbox" class="default-action">
                                <label class="slider" for="colorbar"></label>
                            </div>
                        </div>
                        <div class="input-group" id="led-group">
                          <label for="led_intensity">LED Intensity</label>
                          <div class="range-min">0</div>
                          <input type="range" id="led_intensity" min="0" max="255" value="0" class="default-action">
                          <div class="range-max">255</div>
                        </div>
                        
                    </nav>
                </div>
             </div>                
        </section>
  
  <script>
  document.addEventListener('DOMContentLoaded', function (event) {
     console.log('Start');
     var baseHost = document.location.origin
     var streamUrl = baseHost + ':81'
 
     function fetchUrl(url, cb){
        fetch(url)
          .then(function (response) {
             if (response.status !== 200) {
                cb(response.status, response.statusText);
             }
             else {
                response.text().then(function(data){
                   cb(200, data);
                   })
                   .catch(function(err) {
                      cb(-1, err);
                   });
             }
          })
         .catch(function(err) {
             cb(-1, err);
         });
     }
     
     function setXclk(xclk, cb){
        fetchUrl(`${baseHost}/xclk?xclk=${xclk}`, cb);
     }

     const setXclkButton = document.getElementById('set-xclk')
     setXclkButton.onclick = () => {
        let xclk = parseInt(document.getElementById('xclk').value);
        setXclk(xclk, function(code, txt){
           if(code != 200){
              alert('Error['+code+']: '+txt);
           }
        });
     }
         
     const view = document.getElementById('stream')
     const viewContainer = document.getElementById('stream-container')
     const captureButton = document.getElementById('capture')
     const streamButton = document.getElementById('toggle-stream')
     const flashButton = document.getElementById('toggle-flash')
     const statusButton = document.getElementById('nav-toggle-cb0')
     const saveButton = document.getElementById('save-capture')
     const ledGroup = document.getElementById('led-group')
     let isFlashOn = false; // Flag to track flash state

     const hide = el => {
        el.classList.add('hidden')
     }
     const show = el => {
        el.classList.remove('hidden')
     }

     const disable = el => {
        el.classList.add('disabled')
        el.disabled = true
     }
     const enable = el => {
        el.classList.remove('disabled')
        el.disabled = false
     }

     const stopStream = () => {
       window.stop();
       streamButton.innerHTML = 'Start Stream'
     }
     const startStream = () => {
       view.src = `${streamUrl}/stream`
       show(viewContainer)
       streamButton.innerHTML = 'Stop Stream'
     }
     
     // start stream     
     startStream();
     
     const flash = (value) => {
        const query = value == 0 ? `${baseHost}/flash?flash=0`:`${baseHost}/flash?flash=1`
        fetch(query)
           .then(response => {
              console.log(`request to ${query} finished, status: ${response.status}`)
           })
        flashButton.innerHTML = value == 0 ? 'Flash On':'Flash Off'
     }
      
     const updateValue = (el, value, updateRemote) => {
        updateRemote = updateRemote == null ? true : updateRemote
        let initialValue
        if (el.type === 'checkbox') {
           initialValue = el.checked
           value = !!value
           el.checked = value
        }
        else {
           initialValue = el.value
           el.value = value
        }
        if (updateRemote && initialValue !== value) {
           updateConfig(el);
        }
        else if(!updateRemote){
           if(el.id === "aec"){
              value ? hide(exposure) : show(exposure)
           }
           else if(el.id === "agc"){
              if (value) {
                 show(gainCeiling)
                 hide(agcGain)
              }
              else {
                 hide(gainCeiling)
                 show(agcGain)
              }
           }
           else if(el.id === "awb_gain"){
              value ? show(wb) : hide(wb)
           }
           else if(el.id == "led_intensity"){
              value > -1 ? show(ledGroup) : hide(ledGroup)
           }
        }
     }

     function updateConfig (el) {
        let value
        switch (el.type) {
           case 'checkbox':
              value = el.checked ? 1 : 0
              break
           case 'range':
           case 'select-one':
              value = el.value
              break
           case 'button':
           case 'submit':
              value = '1'
              break
           default:
              return
        }
        const query = `${baseHost}/control?var=${el.id}&val=${value}`
        fetch(query)
           .then(response => {
              console.log(`request to ${query} finished, status: ${response.status}`)
           })
     }

     document
       .querySelectorAll('.close')
       .forEach(el => {
          el.onclick = () => {
             hide(el.parentNode)
          }
       })

     statusButton.onclick = () => {
        fetch(`${baseHost}/status`)
           .then(function (response) {
              return response.json()
           })
           .then(function (state) {
              document
                .querySelectorAll('.default-action')
                .forEach(el => {
                   updateValue(el, state[el.id], false)
                })
           })
    
     }
     
     captureButton.onclick = () => {
        stopStream()
        view.src = `${baseHost}/capture?_cb=${Date.now()}`
        show(viewContainer)
     }

     streamButton.onclick = () => {
        const streamEnabled = streamButton.innerHTML === 'Stop Stream'
        if (streamEnabled) {
           stopStream()
        }
        else {
           startStream()
        }
     }
     
     // Add an onclick event handler to the button.
     flashButton.onclick = () => {
        if (isFlashOn) {
           flash(0)
        }
        else {
           flash(1)
        }
        isFlashOn = !isFlashOn; // Toggle the flag
     }

     saveButton.onclick = () => {
        var canvas = document.createElement("canvas");
        canvas.width = view.width;
        canvas.height = view.height;
        document.body.appendChild(canvas);
        var context = canvas.getContext('2d');
        context.drawImage(view,0,0);
        try {
           var dataURL = canvas.toDataURL('image/jpeg');
           saveButton.href = dataURL;
           var d = new Date();
           saveButton.download = d.getFullYear() + ("0"+(d.getMonth()+1)).slice(-2) + ("0" + d.getDate()).slice(-2) + ("0" + d.getHours()).slice(-2) + ("0" + d.getMinutes()).slice(-2) + ("0" + d.getSeconds()).slice(-2) + ".jpg";
        }
        catch (e) {
           console.error(e);
        }
        canvas.parentNode.removeChild(canvas);
     }


     document
        .querySelectorAll('.default-action')
        .forEach(el => {
           el.onchange = () => updateConfig(el)
        })

     // Custom actions
     
     // Gain
     const agc = document.getElementById('agc')
     const agcGain = document.getElementById('agc_gain-group')
     const gainCeiling = document.getElementById('gainceiling-group')
     agc.onchange = () => {
        updateConfig(agc)
        if (agc.checked) {
           show(gainCeiling)
           hide(agcGain)
        }
        else {
           hide(gainCeiling)
           show(agcGain)
        }
     }

     // Exposure
     const aec = document.getElementById('aec')
     const exposure = document.getElementById('aec_value-group')
     aec.onchange = () => {
        updateConfig(aec)
        aec.checked ? hide(exposure) : show(exposure)
     }

      // AWB
     const awb = document.getElementById('awb_gain')
     const wb = document.getElementById('wb_mode-group')
     awb.onchange = () => {
        updateConfig(awb)
        awb.checked ? show(wb) : hide(wb)
     }
 
   }) // end addEventListener
   </script>
 </body>
</html>
)rawliteral";

bool ServoRotate(uint8_t pin, int8_t angle) { 

 uint16_t us = map(angle, -90, 90, SERVO_uS_LOW, SERVO_uS_HIGH); // convert angle in degree into pulse in micro seconds 
 Serial.print("us: ");Serial.println(us);
    
 uint32_t ticks = us * SERVO_FREQUENCY * SERVO_TIMER_WIDTH_TICKS/1000000;  // convert pulse in micro seconds to ticks
 Serial.print("ticks: ");Serial.println(ticks);
    
 return ledcWrite(pin, ticks);                                     
}

static int print_reg(char *p, sensor_t *s, uint16_t reg, uint32_t mask) {
  return sprintf(p, "\"0x%x\":%u,", reg, s->get_reg(s, reg, mask));
}

static esp_err_t parse_get(httpd_req_t *req, char **obuf) {
  char *buf = NULL;
  size_t buf_len = 0;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      *obuf = buf;
      return ESP_OK;
    }
    free(buf);
  }
  httpd_resp_send_404(req);
  return ESP_FAIL;
}


// Handlers

static esp_err_t index_handler(httpd_req_t *req){
  Serial.println("Start index");
  httpd_resp_set_type(req, "text/html");
  Serial.println("End index");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t status_handler(httpd_req_t *req) {
  static char json_response[1024];

  sensor_t *s = esp_camera_sensor_get();
  char *p = json_response;
  *p++ = '{';

  if (s->id.PID == OV5640_PID || s->id.PID == OV3660_PID) {
    for (int reg = 0x3400; reg < 0x3406; reg += 2) {
      p += print_reg(p, s, reg, 0xFFF);  //12 bit
    }
    p += print_reg(p, s, 0x3406, 0xFF);

    p += print_reg(p, s, 0x3500, 0xFFFF0);  //16 bit
    p += print_reg(p, s, 0x3503, 0xFF);
    p += print_reg(p, s, 0x350a, 0x3FF);   //10 bit
    p += print_reg(p, s, 0x350c, 0xFFFF);  //16 bit

    for (int reg = 0x5480; reg <= 0x5490; reg++) {
      p += print_reg(p, s, reg, 0xFF);
    }

    for (int reg = 0x5380; reg <= 0x538b; reg++) {
      p += print_reg(p, s, reg, 0xFF);
    }

    for (int reg = 0x5580; reg < 0x558a; reg++) {
      p += print_reg(p, s, reg, 0xFF);
    }
    p += print_reg(p, s, 0x558a, 0x1FF);  //9 bit
  }
  else if (s->id.PID == OV2640_PID) {
    p += print_reg(p, s, 0xd3, 0xFF);
    p += print_reg(p, s, 0x111, 0xFF);
    p += print_reg(p, s, 0x132, 0xFF);
  }

  p += sprintf(p, "\"xclk\":%u,", s->xclk_freq_hz / 1000000);
  p += sprintf(p, "\"pixformat\":%u,", s->pixformat);
  p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
  p += sprintf(p, "\"quality\":%u,", s->status.quality);
  p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
  p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
  p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
  p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
  p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
  p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
  p += sprintf(p, "\"awb\":%u,", s->status.awb);
  p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
  p += sprintf(p, "\"aec\":%u,", s->status.aec);
  p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
  p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
  p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
  p += sprintf(p, "\"agc\":%u,", s->status.agc);
  p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
  p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
  p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
  p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
  p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
  p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
  p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
  p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
  p += sprintf(p, "\"colorbar\":%u", s->status.colorbar);
  p += sprintf(p, ",\"led_intensity\":%d", -1);
  *p++ = '}';
  *p++ = 0;
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];
  Serial.println("Start stream");

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }
  
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } 
    else {
       if(fb->width > 400){
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
       }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
    //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
  }
  return res;
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  
  char ts[32];
  snprintf(ts, 32, "%lld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
  httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);
  
  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);

  esp_camera_fb_return(fb);
  fb = NULL;
  return res;
}

static esp_err_t bmp_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/x-windows-bmp");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.bmp");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char ts[32];
  snprintf(ts, 32, "%lld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
  httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

  uint8_t *buf = NULL;
  size_t buf_len = 0;
  bool converted = frame2bmp(fb, &buf, &buf_len);
  esp_camera_fb_return(fb);
  if (!converted) {
    Serial.println("BMP Conversion failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  res = httpd_resp_send(req, (const char *)buf, buf_len);
  free(buf);
  return res;
}

static esp_err_t xclk_handler(httpd_req_t *req) {
  char *buf = NULL;
  char _xclk[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "xclk", _xclk, sizeof(_xclk)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int xclk = atoi(_xclk);
  Serial.print("Set XCLK: "); Serial.print(xclk); Serial.println(" MHz");

  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_xclk(s, LEDC_TIMER_0, xclk);
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t flash_handler(httpd_req_t *req) {
  char *buf = NULL;
  char _flash[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "flash", _flash, sizeof(_flash)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);
  
  int flash = atoi(_flash);
  Serial.print("Set Flash: "); Serial.println(flash);
    
  if (flash == 0) digitalWrite(LED_FLASH_PIN, LOW);
  else digitalWrite(LED_FLASH_PIN, HIGH);

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t control_handler(httpd_req_t *req) {
  char *buf = NULL;
  char variable[32];
  char value[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) != ESP_OK || httpd_query_key_value(buf, "val", value, sizeof(value)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int8_t val = atoi(value);
  Serial.print("variable: "); Serial.println(variable);
  Serial.print("value: "); Serial.println(val);
  
  sensor_t *s = esp_camera_sensor_get();
  int res = 0;
  
  if (!strcmp(variable, "horizontal")) {
    if(!ServoRotate(SERVO_PIN_H, val)) Serial.println("Error ServoRotate horizontal");
  } else if (!strcmp(variable, "vertical")) {
        if(!ServoRotate(SERVO_PIN_V, val)) Serial.println("Error ServoRotate vertical");
  } else if (!strcmp(variable, "framesize")) {
    res = s->set_framesize(s, (framesize_t)val);
  } else if (!strcmp(variable, "quality")) {
    res = s->set_quality(s, val);
  } else if (!strcmp(variable, "contrast")) {
    res = s->set_contrast(s, val);
  } else if (!strcmp(variable, "brightness")) {
    res = s->set_brightness(s, val);
  } else if (!strcmp(variable, "saturation")) {
    res = s->set_saturation(s, val);
  } else if (!strcmp(variable, "gainceiling")) {
    res = s->set_gainceiling(s, (gainceiling_t)val);
  } else if (!strcmp(variable, "colorbar")) {
    res = s->set_colorbar(s, val);
  } else if (!strcmp(variable, "awb")) {
    res = s->set_whitebal(s, val);
  } else if (!strcmp(variable, "agc")) {
    res = s->set_gain_ctrl(s, val);
  } else if (!strcmp(variable, "aec")) {
    res = s->set_exposure_ctrl(s, val);
  } else if (!strcmp(variable, "hmirror")) {
    res = s->set_hmirror(s, val);
  } else if (!strcmp(variable, "vflip")) {
    res = s->set_vflip(s, val);
  } else if (!strcmp(variable, "awb_gain")) {
    res = s->set_awb_gain(s, val);
  } else if (!strcmp(variable, "agc_gain")) {
    res = s->set_agc_gain(s, val);
  } else if (!strcmp(variable, "aec_value")) {
    res = s->set_aec_value(s, val);
  } else if (!strcmp(variable, "aec2")) {
    res = s->set_aec2(s, val);
  } else if (!strcmp(variable, "dcw")) {
    res = s->set_dcw(s, val);
  } else if (!strcmp(variable, "bpc")) {
    res = s->set_bpc(s, val);
  } else if (!strcmp(variable, "wpc")) {
    res = s->set_wpc(s, val);
  } else if (!strcmp(variable, "raw_gma")) {
    res = s->set_raw_gma(s, val);
  } else if (!strcmp(variable, "lenc")) {
    res = s->set_lenc(s, val);
  } else if (!strcmp(variable, "special_effect")) {
    res = s->set_special_effect(s, val);
  } else if (!strcmp(variable, "wb_mode")) {
    res = s->set_wb_mode(s, val);
  } else if (!strcmp(variable, "ae_level")) {
    res = s->set_ae_level(s, val);
  }
  else {
    Serial.print("Unknown command: "); Serial.println(variable);
    res = -1;
  }

  if (res < 0) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}


void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler,
    .user_ctx = NULL
  };
  
  httpd_uri_t control_uri = {
    .uri = "/control",
    .method = HTTP_GET,
    .handler = control_handler,
    .user_ctx = NULL
 }; 
  
 httpd_uri_t capture_uri = {
    .uri = "/capture",
    .method = HTTP_GET,
    .handler = capture_handler,
    .user_ctx = NULL
 }; 
 
 httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
 };
 
 httpd_uri_t bmp_uri = {
    .uri = "/bmp",
    .method = HTTP_GET,
    .handler = bmp_handler,
    .user_ctx = NULL
  };  
  
 httpd_uri_t xclk_uri = {
    .uri = "/xclk",
    .method = HTTP_GET,
    .handler = xclk_handler,
    .user_ctx = NULL
  };
  
  httpd_uri_t flash_uri = {
    .uri = "/flash",
    .method = HTTP_GET,
    .handler = flash_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &control_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &bmp_uri);
    httpd_register_uri_handler(camera_httpd, &xclk_uri);
    httpd_register_uri_handler(camera_httpd, &flash_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

esp_err_t InitCamera()
{
    esp_err_t ret;
    camera_config_t CameraConf;
    
    // Pins configuration
    CameraConf.pin_d0 = 5;
    CameraConf.pin_d1 = 18;
    CameraConf.pin_d2 = 19;
    CameraConf.pin_d3 = 21;
    CameraConf.pin_d4 = 36;
    CameraConf.pin_d5 = 39;
    CameraConf.pin_d6 = 34;
    CameraConf.pin_d7 = 35;
    CameraConf.pin_xclk = 0;
    CameraConf.pin_pclk = 22;
    CameraConf.pin_vsync = 25;
    CameraConf.pin_href = 23;
    CameraConf.pin_sccb_sda = 26;
    CameraConf.pin_sccb_scl = 27;
    CameraConf.pin_pwdn = 32;
    CameraConf.pin_reset = -1;
    
    // timer configuration PWM0
    CameraConf.ledc_channel = LEDC_CHANNEL_0;
    CameraConf.ledc_timer = LEDC_TIMER_0;
    CameraConf.xclk_freq_hz = 20000000; // 20MHZ
    
    // jpeg configuration
    CameraConf.pixel_format = PIXFORMAT_JPEG; //YUV422|GRAYSCALE|RGB565|JPEG
    CameraConf.frame_size = FRAMESIZE_VGA; // QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    CameraConf.jpeg_quality = 10; // 63 (low quality) -> 0 (high quality)
    CameraConf.fb_count = 2; // 2 buffers

    ret=esp_camera_init(&CameraConf);
    if (ret == ESP_OK)
    {
        Serial.println("camera init OK");
    }
    else
    {
        Serial.print("Erreur lors de l'initialisation de la camera, error: ");Serial.println(ret);
    }
    return(ret);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  pinMode(on_board_red_LED, OUTPUT);  // on-board red LED
  
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  // Servos init
  if (!ledcAttach(SERVO_PIN_H, SERVO_FREQUENCY, SERVO_RESOLUTION)) {
    Serial.printf("Servo horizontal init failed");
    return;
  }
  
  if (!ledcAttach(SERVO_PIN_V, SERVO_FREQUENCY, SERVO_RESOLUTION)) {
    Serial.printf("Servo vertical init failed");
    return;
  }
  
  // Camera init
  esp_err_t err = InitCamera();
  if (err != ESP_OK) {
    Serial.print("Camera init failed with error: "); Serial.println(err);
    return;
  }
  
  // Led flash init
  pinMode(LED_FLASH_PIN, OUTPUT);

   
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  
  digitalWrite(on_board_red_LED, HIGH);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(on_board_red_LED, !digitalRead(on_board_red_LED));
    Serial.print(".");
  }
  digitalWrite(on_board_red_LED, LOW);
  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());
  
  // Start streaming web server
  startCameraServer();
}

void loop() {
  
}
