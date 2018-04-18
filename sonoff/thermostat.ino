/*
  thermostat.ino - sonoff TH hard thermostat support for Sonoff-Tasmota

  Copyright (C) 2017  Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*********************************************************************************************\
 * Hard Thermostat
 *
 * Colaboration of nambuco / Paulo H F Alves
\*********************************************************************************************/

/* Modified for bathroom humidity sensor and exhaust fan switch. March 2018
Switch turns on/off above/below setpoint humidity
Use of physical button or switch to turn the power on or off will disable the
automatic operation for a period of LOCKOUT_DELAY seconds (set below), after which
automatic operation will resume.
This is intended for a Sonoff TH16 - with a single relay only.
Modifications: Andrew Quested - andrew@quested.com.au
*/

enum ThermoCommands {
    CMND_THERMOSTAT, CMND_SETPOINT, CMND_LOCKOUT };
const char kThermoCommands[] PROGMEM =
    D_CMND_THERMOSTAT "|" D_CMND_SETPOINT "|" D_CMND_LOCKOUT ;

enum ThermoStates {
    STATE_ON, STATE_LOCKOUT, STATE_OFF, STATE_DISABLED};

byte thermo_state = STATE_DISABLED;
uint16 thermo_timer = 0;
uint16 LOCKOUT_DELAY = Settings.lockout;

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

boolean ThermoCommand(char *type, uint16_t index, char *dataBuf, uint16_t data_len, int16_t payload)
{
  char command [CMDSZ];
  char sunit[CMDSZ];
  boolean serviced = true;

  int command_code = GetCommandCode(command, sizeof(command), type, kThermoCommands);
  if (CMND_THERMOSTAT == command_code) {
    snprintf_P(log_data, sizeof(log_data), PSTR("Thermostat command received: %d"), payload);
    AddLog(LOG_LEVEL_DEBUG);
    if ((payload == 0) || (payload == 1)) {
      Settings.thermo = payload;
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.thermo);
    }
  }
  else if (CMND_SETPOINT == command_code) {
    snprintf_P(log_data, sizeof(log_data), PSTR("Setpoint command received: %d"), payload);
    AddLog(LOG_LEVEL_DEBUG);
      if ((payload >= 0) && (payload <= +100)) {  //values are modified to be appropriate for humidity %
        Settings.thermo_setpoint = payload;
      }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command,  Settings.thermo_setpoint);
  }
  else if (CMND_LOCKOUT == command_code) {
    snprintf_P(log_data, sizeof(log_data), PSTR("Lockout comand received: %d"), payload);
    AddLog(LOG_LEVEL_DEBUG);
      if (payload >= 30) {  //Minimum lockout = 30(sec)
        Settings.lockout = payload;
      }
      LOCKOUT_DELAY = Settings.lockout;
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command,  LOCKOUT_DELAY);
  }
  else {
    serviced = false;
  }
  return serviced;
}

void ThermoLockout()  //Begins a period during which the automatic 'thermo' feature will not operate
{
  thermo_timer = 0;
  thermo_state = STATE_LOCKOUT;
  snprintf_P(log_data, sizeof(log_data), PSTR("Thermo Lockout | State %d | DevicePower %d"), thermo_state, Settings.power);
  AddLog(LOG_LEVEL_DEBUG);
}

// Hard Thermostat main function
void ThermoFunction ()
{
  float t;
  float h;
  char temperature[10];
  char humidity[10];
  thermo_timer ++;
    if (DhtReadTempHum(0, t, h)) {     // Read temperature
      dtostrfd(t, Settings.flag2.temperature_resolution, temperature);
      dtostrfd(h, Settings.flag2.humidity_resolution, humidity);
      snprintf_P(log_data, sizeof(log_data), PSTR("Hard Humidity Setpoint %d | Current %s | State %d | Power %d | Timer %d"), Settings.thermo_setpoint, humidity, thermo_state, Settings.power, thermo_timer);
      AddLog(LOG_LEVEL_DEBUG);

      //Update enabled or disabled status
      if (Settings.thermo==1 && thermo_state == STATE_DISABLED){
        thermo_state = STATE_OFF;
        thermo_timer = 0;
      }
      if (Settings.thermo==0 && thermo_state != STATE_DISABLED){
        thermo_state = STATE_DISABLED;
        thermo_timer = 0;
      } // End update enabled or disabled status

      //Update delay status
      if (thermo_state == STATE_LOCKOUT) {
        if (thermo_timer > LOCKOUT_DELAY) {
          snprintf_P(log_data, sizeof(log_data), PSTR("Lockout period expired - automatic control resumed."));
          AddLog(LOG_LEVEL_DEBUG);
          thermo_state = STATE_OFF;
          thermo_timer = 0;
          LOCKOUT_DELAY = Settings.lockout;
        }
      }// end update delay status

     //Operate 'thermostat' - we are not disabled, and not in a delayed state
     if (thermo_state!=STATE_LOCKOUT && thermo_state!=STATE_DISABLED){
        if (h<Settings.thermo_setpoint && Settings.power==1){
          ExecuteCommandPower(1,0); // Humidity is low and power is on, so turn power off
          thermo_state = STATE_OFF;
        }
        if (h>Settings.thermo_setpoint && Settings.power==0){
          ExecuteCommandPower(1,1); // Humidity is high and power is off, so turn power on
          LOCKOUT_DELAY = 60; // Set a temporary 60sec delay, so minimum on time is 60 sec
          thermo_timer = 0;
          thermo_state = STATE_LOCKOUT;
        }
      } // End operate thermostat

    } // end if DhtReadTempHum
} // end ThermoFunction
