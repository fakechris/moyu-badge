// main/serial_time.h —— console "TIME <epoch>" listener (flash-time sync).
#pragma once

// Start the console reader task. Hosts (tools/flash.sh settime) send
// "TIME <unix-epoch>\n"; the device settimeofday()s and acks "TIME OK".
void serial_time_start(void);
