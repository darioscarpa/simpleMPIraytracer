Simple ray tracer - MPI enhanced
--------------------------------

April 2010, by Dario Scarpa
---------------------------
www.duskzone.it


This is a MPI parallelization of a simple, minimal raytracer by Nicholas Chapman (thanks man!).

You can check his original work at
http://homepages.paradise.net.nz/nickamy/simpleraytracer/simpleraytracer.htm

You might find interesting to diff his project folder with mine to see where I had to change and what I had to add.

I also include his original readme in the archive (readme_original.txt)


This project has been developed for a "Parallel and Concurrent Programming" course at Università di Salerno, Italy.
I have to study for many other exams, so I can't use time to properly comment and explain my implementation, 
but I try to act as a good guy, so I'll leave you a few notes, and if you're intersted the code will tell you more.
Feel free to e-mail me, btw.

Basically, I hacked the original code to better separate ray-tracing calculation and visualization, and then
parallelized the calculation part using MPI. 
While experimenting with the library, I tried different approaches using different MPI features (collective 
communication, asynchronous communication, custom data types definition etc).

The code should be easily understable and a lot C-like 'cause of my quite limited C++ skills.
I used the C MPI binding instead of the C++ one, too.

In short, the root node updates the scene, broadcasts the update to workers, and then waits for rendered frame
blocks to show together on screen.
How the work is split and put back together at the root node is where the strategies differ.

This is accomplished using inheritance and polymorphism (and that's where the C++ gets in).
The base "Par" class does the common work. The other ones implement a strategy.
The numbers near the concrete class names in the diagram below are the same used in the command line parameter
"strategy" of the application. 

                                 Par
       ___________________________|_____________________________________________
      |                 |                                |                      |
(0)ParSequential  (1)ParSplitAndGather          ParMasterWorkers        (6)ParSplitAdaptive
                        |	                  ______ |_______________________________
                  (2)ParSplitAndGatherV          |                     |                 |  
                                   (3)ParMasterWorkersSimple  (4)ParMasterWorkersAsync  (5)ParMasterWorkersPasv

It's easy to add a strategy to try diffrent stuff, as long as you implement the interface defined by "Par".
You just need to define the new class and instantiate it properly, and virtual calls will do the magic.
				

You can found other info in the (italian only) notes_ita.txt file included in the archive.
When trying the application, run "winsimpleraytracer --help" to see command line parameters info and get started.


Remember that you need to link to some MPI implementation to build the project. 
I used DeinoMPI and Visual C++ 2008 Express Edition.


Have fun!

Dario