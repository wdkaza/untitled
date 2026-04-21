# untitled (alpha 0.0.1)
A minimal wayland compositor, insipired heavily by dwl and its forks like mangowc, and newm, hevel

Why "untitled"? i havent decided on the name yet for this project, im still not sure what to name it

"untitled" is currently in alpha, that means that you probably wouldnt be able to you use as a daily<br>
At the time of writting, the functionallity of the wm is just moving/resizing the windows, closing windows, fullscreen, and thats it<br>
Thats because pre-alpha focused on making the window manager support as many essential things for a window manager to even work, and support most window types

The structure is very similar to dwl, in some parts its juts dwl code, although its not a direct fork of dwl, why rewrite most functions(like some protocols),
when they are already done in the best way possible, also i just really like dwl's code structure, its easy to work with

# Whats to come in the alpha

I have a lot of things that i would like to do to this compositor, i will list below the most important one's

Custom renderer - Even though its in the code and seems to work, its not in the best state and requires a lot of work, and pretty much currently there as a POC
finishing it would unlock eye-candy(rounded corners, blur, shadows), anything you can do with a shader, i would aim to make it customizable, so you could change the shaders easily 

Animations - Simple animations for all the states(opening, swapping windows, fullscreen, etc) of the window

Custom config format - Going away from .h config and making it just a file with options that will be parsed,
also recompiling the config without restarting the compositor

Tiling/Canvas - is probably going to be the last in order of adding things, im still thinking about it,
and i want everything else listed above finished before making the compositor daily-friendly

# Build & Run

It would be as simple as installing some dependencies, and running

``` sh
make
```

```
./main
```

BUT i was too lazy to setup the makefile, and currently the makefile is hardcoded for my laptop 
since the compositor is barely usable with its limited features, and no one will want to use it,
the makefile is not really necessary right now

# Features

This is the list of all the features available in the compositor, also serves as a TODO list


- [x] Basic window managment (moving/resizing)
- [x] Popups (a few annoying bugs left)
- [x] Fullscreen
- [x] Awesome multi-monitor support
- [x] Bars support (only tested with wmenu so far)
- [x] Xwayland support
- [x] Layers support
- [x] Zooming
- [ ] Cavnas
- [ ] Desktops
- [ ] Keybind to switch focus between windows
- [ ] Window borders
- [ ] Damage for the renderer(no idea how to do)
- [ ] Renderer fallback to wlroots renderer
- [ ] Testing if turning off Xwayland wont load x11 code upon compiling

......... and more to come, i will expand this list with time

# Bugs

This is more of a list to myself than to anyone else, just reminding myself what bugs are in the compositor currently,
also to not have it too long i just remove the bugs i fix from the list instead of checking the box

- [ ] popups are broken, they just dont behave sometimes
- [ ] mon-rules scale is broken, cant scale your monitor
- [ ] having 2 monitors in the config, and only 1 plugged it would still allow the windows to be on the second monitor(maybe not so much of a bug)
- [ ] if you spawn a nested compositor you cant move it, probably due to input masking or whatever it was called but who knows why, will test later
- [ ] dnd(drag&drop) image is gone, doesnt render
- [ ] while unzoomed/zoomed popups spawn in wrong positions
- [ ] canvas needs a lot of testing, currently moves the wallpaper around instead of windows, its intentional, lol, still a bug
