# KiloGrid_IRIDIA

## Starting Everything 

### Starting the KiloGui

First you have to open the gui project. Its located at 

```
C:\kilogrid_software\kilogrid_software\src\KiloGUI
```

Then open kilogui (should be a Qt Project file). 
This should then open the Qt editor. 
There you have to press play (green triangle in bottom left corner).
(The version you should run is 32 bit debug.) 

This opens Kilobots Toolkit.


### Start an experiment

In the Kilobots Toolkit window:
Make sure big arena is selected. 

Select configuration:
Here you can select your .kconf for your expeiment. Normally located at

```
Experiments/<your_experiment>/<your_conf>.kconf
```

Upload Kilogrid Program:
Here you can select the controller for the kilogrid module. Normally it should be located at 

```
Experiments/<your_experiment>/module/build/<your_module>.hex
```

After selecting the files 
1. press bootload (kilogrid should blink blue)
2. press upload (kilogrid should blink green/red and after successfully uploading should breath in white)
3. press setup (should stop breathing)
4. press run (should run the experiment)
5. press stop to quit the experiment 

### setup the robots

While the kilogrid is running you cannot configere the robots, thus you have to do it before you start the kilogrid-experiment.
You can do the configuration of the robot after pressing setup (test if this is really the case!).

Upload Kilobot Program:
Here you can select the controller which should be executed on the kilobot. Normally located at

```
Experiments/<your_experiment>/Kilobot/build/<your_kilo>.hex
```

After file selection place the robots on the kilogrid (in the middle of the module - makes it quicker - the robot sees all 4 cells).

1. press bootloading - the robots and the kilogrid should light up blue
2. press upload - the kilogrid blinks blue green, if the kilobots blink green the upload was successful 
3. press reset (at Kilobot Commands) - this resets everything 
4. press run - robots start to run their controller 

After the controllers started you can run the kilogrid and start your experiment.


## Compiling Code 
This is an instruction on how to compile code on the Computer which is connected to the kilogrid.

First start the app "Ubuntu".

This opens a terminal.
From there you can go to your experiment.
Here you can see how to compile the Module controller (aka the controller for the kilogrid).

```
cd /mnt/c/kilogrid_software/kilogrid_software/src/Experiments/<your_experiment>/Module/
make all
```

Next you can compile the kilobot controller like

```
cd /mnt/c/kilogrid_software/kilogrid_software/src/Experiments/<your_experiment>/Kilobot/
make all
```

## Calibrating robots
TODO

## Creating a new project 
TODO



## Other usefull actions 
### Check battery status
In the Kilobots Toolkit window you can check the current status of charge by pressing voltage at Kilobot Commands > Voltage. Then the robots should light up somewhat purple/orange?? pressing it again shows than the battery status. 



## File description

### .kconf 
This files handles the initial configuration for the module/cells of the kilogrid.

The structure should look something like 

```
address
y:4

data 
0x1
0x2
0x3
0x4

```

This is one example building block. 
Also you can use different addressing like 

```
y:10-11
module:0-1
x:10-11
```



## General advice for running experiments 

Do not touch the robots while they charge. First turn of the charging station, than wait for 30 sec, than touch them. Otherwise it could harm the robot.

Charge the robot if its battery is yellow or below yellow. 
- robots should be charged in sleep mode
- the charge level is: 
-- green (fully charged)
-- blue (fine)
-- yellow (charge!)
-- red (should not happen)
-- black = dead
- charge the robots for 3 hours (not over night)

Compile size of the code (kilogrid & robots) should not exceed the space, e.g., should be under 100%

turn on/off = plug jumper 

states
- sleep: blink every 8 s
- reset: should be green, when blinking green it does not receive a message
- ...

The robots need to run before the kilogrid, e.g., start the robot controller, than the kilogrid controller (probably you have to send a start/init msg to the robots)





