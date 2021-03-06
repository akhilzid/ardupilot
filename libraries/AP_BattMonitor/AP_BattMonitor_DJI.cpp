#include <AP_HAL/AP_HAL.h>
#include "AP_BattMonitor_DJI.h"
#include <GCS_MAVLink/GCS.h>
#include <AP_SerialManager/AP_SerialManager.h>
#include <AP_Math/crc.h>

/*
  "battery" monitor for DJI Battery via serial port.

   For future Referance:
   Total Voltage : 8,9
   Percentage : 17
   Cell1 :  20,21
   Cell2 :  22,23
   Cell3 :  24,25
   Cell4 :  26,27
   
 */
extern const AP_HAL::HAL& hal;

 #define REQUEST_INTERVAL_US      100000     // Battery info requesting delay

/// Constructor
AP_BattMonitor_DJI::AP_BattMonitor_DJI(AP_BattMonitor &mon, AP_BattMonitor::BattMonitor_State &mon_state, AP_BattMonitor_Params &params) :
    AP_BattMonitor_Backend(mon, mon_state, params)
{
    const AP_SerialManager &serial_manager = AP::serialmanager();
    
    if ((port = serial_manager.find_serial(AP_SerialManager::SerialProtocol_DJIBattery, 0))) {
        last_send_us = AP_HAL::micros();
    } else { 
        gcs().send_text(MAV_SEVERITY_WARNING,"DJIBattery: Port not available or not configured properly");
    }
    // need to add check
    _state.healthy = false;
}

/*
  read - read the "voltage" and "percentage"
*/
void AP_BattMonitor_DJI::read()
{
    if (port != nullptr){
        uint32_t now = AP_HAL::micros();
        if (last_send_us != 0 && now - last_send_us > REQUEST_INTERVAL_US) {
            
            uint8_t buf[14] {0xAB, 0x0E, 0x04, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8}; 
            port->write(buf, sizeof(buf)); // Requesting Battery info
            count = 0; // This will reset and sync pktbuf after each reset
            last_send_us = now;
        }
    
        uint32_t n = port->available();
    
        for (uint8_t i=0; i<n; i++) {
            pktbuf[count++] = port->read();
            if (count == 37){
                if ( pktbuf[count - 1] == crc_crc8( pktbuf, 36)){
                    
                    _state.capacity_pct = pktbuf[17];
                    _state.voltage = ((pktbuf[8] << 8) | pktbuf[9]) * 1e-3f;

                    _state.cell_voltages.cells[0] = (((pktbuf[20] << 8) | pktbuf[21]) * 1e-3f)*1000;
                    _state.cell_voltages.cells[1] = (((pktbuf[22] << 8) | pktbuf[23]) * 1e-3f)*1000; 
                    _state.cell_voltages.cells[2] = (((pktbuf[24] << 8) | pktbuf[25]) * 1e-3f)*1000;
                    _state.cell_voltages.cells[3] = (((pktbuf[26] << 8) | pktbuf[27]) * 1e-3f)*1000;

                    _state.last_time_micros = now;
                    _state.healthy = true;
                } else {
                    _state.healthy = false;
                }
                count = 0;
            }
        }
        
    }
    
    
}
