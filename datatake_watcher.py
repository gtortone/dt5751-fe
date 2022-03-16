"""
Watch over data take to minimize downtime.
It opens up a hotlink/callback that will be called when a run begins.

In this case, the client in a `while True` loop runs "forever" and
raises a warning if the data logging is disabled and an alarm if 
the event rate is 0.
(the script can still be safely killed by Ctrl-C  or from the midas
Programs webpage).

A. Capra
October 2021
"""

import midas.client
import time

def data_callback(client, path, odb_value):

    if odb_value == midas.STATE_RUNNING:

        log = client.odb_get("/Logger/Write data")
        if log == 0:
            client.trigger_internal_alarm("Data Logging","Writing to disk is DISABLED","Warning")
            
        time.sleep(5)
        event_rate=client.odb_get("/Equipment/V1725_Data00/Statistics/Events per sec.")
        if event_rate < 1:
            client.trigger_internal_alarm("Event Rate",f"Event rate is {event_rate:1.0f} Hz")
        else:
            client.reset_alarm("Event Rate")
  

if __name__ == "__main__":

    client = midas.client.MidasClient("datatake_watcher")
    
    # Setup a callback function. Note the paramters of `my_odb_callback`
    # match what is specified in the `odb_watch` documentation.
    client.odb_watch("/Runinfo/State", data_callback)
    
    # Infinite Loop
    while True:
        # If the ODB value has changed, our callback function will
        # be called as part of `client.communicate()`. If the ODB
        # value hasn't changed, this just acts like a 10ms sleep.
        client.communicate(10)
        
    # We must stop watching the ODB before we can delete the entry we created.
    client.odb_stop_watching("/Runinfo/State")
    
    # Disconnect from midas before we exit.
    client.disconnect()
