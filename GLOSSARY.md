# Vision glossary

| Term | Meaning in this project |
|---|---|
| Upper computer | This Linux/Windows C++ application: camera input, detection, tracking, planning, and communication. |
| MC-02 state frame | The lower-computer-to-vision 43-byte SP frame carrying mode, quaternion, gimbal state, and CRC16/X25. |
| Vision command | The vision-to-lower-computer 29-byte SP frame carrying mode, aim values, and CRC16/X25. |
| Neutral command | A mode-zero frame with zeroed aim values; it revokes vision control. |
| Bare-board HIL | Communication and parsing validation with the MC-02 board and debugger attached and all actuators disconnected. |
