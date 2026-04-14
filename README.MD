# untitled (pre-aplha 0.1.10)
A minimal wayland window manager inspired by dwl  
Why "untitled"? The state of it is so early im still not sure of what to call it and i dont know what i want it to be, its just your another window manager at the moment  
sort of a fork of dwl but im just using dwl as a kickstart for most pieces of code
And well since its not even an alpha yet, wont write much here, but my TODO list  

# TODO
- [x] simple shaders support :o
- [x] basic window managment
- [x] popups(pretty much done a few fixes left)
- [x] moving/resizing windows
- [x] fullscreen feature
- [x] amazing monitor layout support(no scale for now)
- [x] basic config in a .h file for now
- [x] layers support
- [x] bars(waybar, etc) support(only half done)
- [x] xwayland support(highest priority(DONE!!))
- [ ] fixing the remaining bugs for pre-aplha
- [ ] remake the README for an alpha version
- [ ] making it stable and usable, trying to daily drive

these are the goals for it making this window manager to an alpha stage, after doing all that  
i would love to try making animations, ~~adding scenefx support~~ custom renderer! and more

# bugs
- [ ] bugs
- [ ] some windows(mostly any game from steam) is a pain to close/exit, sometimes will leave the shader/window behind
- [ ] when 2 monitors exist in monrules, windows will spawn on the second mon(bug on itself should spawn on first), even if the monitor isnt plugged in
- [ ] mon-rules scale is broken, mouse scales, windows dont
- [ ] you can fullscreen popups, and "focused_client" could be given to popups as well, example: steam popups when you hover over store
