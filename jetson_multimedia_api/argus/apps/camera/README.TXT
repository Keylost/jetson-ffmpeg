===============================================================================
Argus Camera Application
===============================================================================

Overview
========

Design
------

The main design goal was to create a modular camera application. Functionality like video recording
or image capturing is encapsulated in modules which easily can be added to the application.
There is a central entity, the 'Dispatcher', managing the state, like the current device, sensor
mode and many others. The 'Dispatcher' also provides functionality to simplify using Argus from
the modules.

Directory structure
-------------------

common
    Shared functionality like mutex, timing, conditional variable. The app also uses shared
    code from the 'samples/utils' directory.
docs
    Doxgen documentation.
modules
    Functional entities, includes the Dispatcher, XML config file reading/writing, perf tracking.
modules/tasks
    Tasks provide functionality like image capturing or video recording.
renderer
    Provides support for rendering and composing EGL streams.
ui
    User interface implementations, includes command line and GUI code.
ui/camera
    Camera main application code.
ui/capture
    Capture application module UI implementation.
ui/common
    Base application and application module classes.
ui/gallery
    Gallery application module UI implementation.
ui/multiExposure
    Multi exposure application module UI implementation.
ui/multiSession
    Multi session application module UI implementation.
ui/video
    Video application module UI implementation.

Usage
=====

Command Line
------------

To get information on command line usage open a terminal and type
    argus_camera --help

Keyboard
--------

To get information on keyboard usage open a terminal and type
    argus_camera --help
Keys are only recognized if the preview area has focus.

Graphical User Interface (GUI)
------------------------------

All command line parameters and actions can also be executed using the GUI.