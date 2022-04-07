# UDP-Unreal
Convenience ActorComponent UDP wrapper for Unreal Engine 4.

[![GitHub release](https://img.shields.io/github/release/getnamo/UDP-Unreal.svg)](https://github.com/getnamo/UDP-Unreal/releases)
[![Github All Releases](https://img.shields.io/github/downloads/getnamo/UDP-Unreal/total.svg)](https://github.com/getnamo/UDP-Unreal/releases)

This may not be the most sensible wrapper for your use case, but is meant to co-exist with https://github.com/getnamo/SocketIOClient-Unreal with similar workflow. 

Wraps built-in ue4 udp functionality as an actor component (_UDPComponent_) with both sending and receiving capabilities. Works through the c++ _FUDPNative_ wrapper which can be included and re-linked in a custom non actor component class if desired. 

Confirmed working with node.js [dgram](https://nodejs.org/api/dgram.html) (see [example echo server gist](https://gist.github.com/getnamo/8117fdc64209af086ce0337310c52a51) for testing).

## Quick Install & Setup

 1. [Download Latest Release](https://github.com/getnamo/UDP-Unreal/releases)
 2. Create new or choose project.
 3. Browse to your project folder (typically found at Documents/Unreal Project/{Your Project Root})
 4. Copy *Plugins* folder into your Project root.
 5. Plugin should be now ready to use.
 
## How to use - Basics
 
Select an actor of choice. Add UDP component to that actor.
 
![add component](https://i.imgur.com/EnCiU4K.png)
 
Select the newly created component and modify any default settings

![defaults](https://user-images.githubusercontent.com/542365/112784196-dc49ea80-9005-11eb-9d2c-a53384168be1.png)

By default the udp actor component will auto open both send and receive sockets on begin play. If you're only interested in sending, untick should auto open receive; conversely untick auto open send if you're not interested in sending.
 
Also if you want to connect/listen on your own time, untick either and connect manually via e.g. key event
 
![manual open receive](https://i.imgur.com/HkSvGCb.png)
 
Receive Ip of 0.0.0.0 will listen to all connections on specified port.
 
### Sending
 
Once your sending socket is opened (more accurately prepared socket for sending, since you don't get a callback in UDP like in TCP), use emit to send some data, utf8 conversion provided by socket.io plugin. NB: if you forget to open your socket, emit will auto-open on default settings and emit.
 
![emit](https://i.imgur.com/OzG0caw.png)
 
returns true if the emit processed. NB: udp is unreliable so this is not a return that the data was received on the other end, for a reliable connection consider TCP.
 
### Receiving
 
![events](https://i.imgur.com/1mdlQdI.png)
 
Once you've opened your receive socket you'll receive data on the ```OnReceivedBytes``` event
 
![receive bytes](https://i.imgur.com/1Lq0mDg.png)
 
which you can convert to convenient strings or structures via socket.io (optional and requires your server sends data as JSON strings).

#### Receiving on Bound Send port

Since v0.9.5 when you open a send socket it will generate a bound send port which you can use to listen for udp events on the receiving side. This should help NAT piercing due to expected behavior.

To use this feature can use _Should Open Receive To Bound Send Port_ which will cause any receive open to automatically bind to your send ip and send bound port.

![auto open bound send port](https://user-images.githubusercontent.com/542365/112778515-9129da80-8ff9-11eb-93a3-129c00a8da47.png)

Or if you want to manually do this you can untick _Should Auto Open Receive_ and then open with own settings on e.g. send socket open event with the bound port.

![open bound send port](https://user-images.githubusercontent.com/542365/112771022-7c8c1900-8fde-11eb-971e-e81c3d4e55cd.png)
 
### Reliable Stream
 
Each release includes the socket.io client plugin, that plugin is intended to be used for reliable control and then real-time/freshest data component of your network can be piped using this udp plugin. Consider timestamping your data so you can know which packets to drop/ignore.

## Packaging

### C++
Works out of the box.

### Blueprint
If you're using this as a project plugin you will need to convert your blueprint only project to mixed (bp and C++). Follow these instructions to do that: https://allarsblog.com/2015/11/04/converting-bp-project-to-cpp/

![Converting project to C++](https://i.imgur.com/Urwx2TF.png)

e.g. Using the File menu option to convert your project to mixed by adding a C++ file.

## Notes
MIT licensed.

Largely inspired from https://wiki.unrealengine.com/UDP_Socket_Sender_Receiver_From_One_UE4_Instance_To_Another.
