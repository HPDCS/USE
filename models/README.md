Writing a USE model
===================

Defining a simulation model for USE is simple as writing a switch case in C.

USE provides four main API:

* ProcessEvent
* OnGVT
* ScheduleNewEvent
* SetState

Two of them are callbacks that allow users to execute code from the model.
The remaining two are API for interacting with the simulation engine.

* ProcessEvent:
    This is a callback that allows users to specify the logic behind a simulation model.
    Its role is the one of processing events targeting a specific simulation object (also denoted as logical process).
    Each event should be associated with a specific type. Consequently, the ProcessEvent routing should contain the event hadler for each event type.
    There is a special type of events called INIT, that allows users to initialiaze the simulation model.

* ScheduleNewEvent:
    This API allows to create a new event targeting any simulation object and to inject it into the simulation.

* SetState:
    In USE, memory for simulation- object state is allocated via malloc API. 
    The state of a simulation object can have any layout a user needs.
    However, it is required that all allocated memory can be accessed by starting from a unique root pointer.
    The SetState API allows to associate such a pointer to the simulation object.
    USE will than use this root pointer to keep track of allocation and to properly manage rollbacks, garbage collections and checkpoints autonomously.

* OnGVT:
    All models must implement this function. It receives a read-only state of the targeted simulation object. Its main goal is inspecting the simulation state and communicate to the platform (by returning true) if the simulation can end according to targeted simulation object. The simulation ends when all simulation objects can end.
