About
=====

Titan is a platform which supports fast and accurate performance evaluation 
of networked systems using conjoined emulation-simulation models. This is a 
cost-effective modelling approach where some components of the system are 
simulated while others are emulated i.e, their actual code is directly 
executed. For operational correctness, all emulated and simulated components 
of such conjoined models must be time-synchronized to a virtual clock.

Titan uses a fast and accurate virtual time synchronization approach which 
relies on compiler analysis to augment emulated code with logic for precise 
instruction level tracking of execution. This is combined with a mechanism 
to ascribe virtual time for each execution burst based on the sequence of 
executed instructions. Compiler analysis is also used to predict future 
network activity and enables continuous online exchange of these predictions 
between emulated and simulated portions of a conjoined model. These forecasts, 
called lookahead, are in-turn used to significantly increase the conjoined 
model execution speed. 


