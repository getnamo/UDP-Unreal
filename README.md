# udp-ue4
Convenience ActorComponent UDP wrapper for Unreal Engine 4.

[![GitHub release](https://img.shields.io/github/release/getnamo/udp-ue4.svg)](https://github.com/getnamo/udp-ue4/releases)
[![Github All Releases](https://img.shields.io/github/downloads/getnamo/udp-ue4/total.svg)](https://github.com/getnamo/udp-ue4/releases)

This may not be the most sensible wrapper for your use case, but is meant to co-exist with https://github.com/getnamo/socketio-client-ue4 with similar workflow.

Wraps built-in ue4 udp functionality as an actor component with both sending and receiving capabilities. Confirmed working with node.js [dgram](https://nodejs.org/api/dgram.html) (see [example echo server gist](https://gist.github.com/getnamo/8117fdc64209af086ce0337310c52a51) for testing).

## Quick Install & Setup

 1. [Download Latest Release](https://github.com/getnamo/udp-ue4/releases)
 2. Create new or choose project.
 3. Browse to your project folder (typically found at Documents/Unreal Project/{Your Project Root})
 4. Copy *Plugins* folder into your Project root.
 5. Plugin should be now ready to use.
 
 ## How to use - Basics
 
 Select an actor of choice. Add UDP component to that actor.
 
 ![add component](https://i.imgur.com/EnCiU4K.png)
 
 Select the newly created component and modify any default settings
 
 ![defaults](https://i.imgur.com/nANqpPF.png)
 
 e.g. if you're only interested in sending, untick should auto-listen and modify your send ip/port to match your desired settings. Conversely you can untick auto-connect if you're not interested in sending.
 
 Also if you want to connect/listen on your own time untick both and connect manually via e.g. key event
 
 ![manual connect](https://i.imgur.com/HVrsO2p.png)
 
 ### Sending
 
 Once you've auto-connected (more accurately prepared sending socket, since you don't get a callback in UDP like in TCP) or manually connected to the sending socket, use emit to send some data, utf8 conversion provided by socket.io plugin.
 
 ![emit](https://i.imgur.com/3EIT8TL.png)
 
 ### Receiving
 
 ![events](https://i.imgur.com/IRE54zq.png)
 
 Once you've started listening to data you'll receive data on the ```OnReceivedBytes``` event
 
 ![receive bytes](https://i.imgur.com/YCEUCkW.png)
 
 which you can convert to convenient strings or structures via socket.io (optional).
 
 ### Reliable Stream
 
 Each relase includes the socket.io client plugin, that plugin is intended to be used for reliable control and then real-time/freshest data component of your network can be piped using this udp plugin. Consider timestamping your data so you can know which packets to drop/ignore.


## Notes
MIT licensed.

Largely inspired from https://wiki.unrealengine.com/UDP_Socket_Sender_Receiver_From_One_UE4_Instance_To_Another.
