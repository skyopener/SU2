/*!
 * \file SU2_DEF.cpp
 * \brief Main file of Mesh Deformation Code (SU2_DEF).
 * \author F. Palacios
 * \version 3.2.9 "eagle"
 *
 * SU2 Lead Developers: Dr. Francisco Palacios (fpalacios@stanford.edu).
 *                      Dr. Thomas D. Economon (economon@stanford.edu).
 *
 * SU2 Developers: Prof. Juan J. Alonso's group at Stanford University.
 *                 Prof. Piero Colonna's group at Delft University of Technology.
 *                 Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *                 Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *                 Prof. Rafael Palacios' group at Imperial College London.
 *
 * Copyright (C) 2012-2015 SU2, the open-source CFD code.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/SU2_DEF.hpp"
using namespace std;

int main(int argc, char *argv[]) {
  
  unsigned short iZone, nZone = SINGLE_ZONE;
  double StartTime = 0.0, StopTime = 0.0, UsedTime = 0.0;
  char config_file_name[MAX_STRING_SIZE];
  int rank = MASTER_NODE, size = SINGLE_NODE;
  string str;

  /*--- MPI initialization ---*/

#ifdef HAVE_MPI
  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
#endif
  
  /*--- Pointer to different structures that will be used throughout the entire code ---*/
  
  CConfig **config_container         = NULL;
  CGeometry **geometry_container     = NULL;
  CSurfaceMovement *surface_movement = NULL;
  CVolumetricMovement *grid_movement = NULL;
  COutput *output                    = NULL;

  /*--- Load in the number of zones and spatial dimensions in the mesh file (if no config
   file is specified, default.cfg is used) ---*/
  
  if (argc == 2){ strcpy(config_file_name,argv[1]); }
  else{ strcpy(config_file_name, "default.cfg"); }
  
  /*--- Definition of the containers per zones ---*/
  
  config_container = new CConfig*[nZone];
  geometry_container = new CGeometry*[nZone];
  output   = new COutput();

  for (iZone = 0; iZone < nZone; iZone++) {
    config_container[iZone]       = NULL;
    geometry_container[iZone]     = NULL;
  }
  
  /*--- Loop over all zones to initialize the various classes. In most
   cases, nZone is equal to one. This represents the solution of a partial
   differential equation on a single block, unstructured mesh. ---*/
  
  for (iZone = 0; iZone < nZone; iZone++) {
    
    /*--- Definition of the configuration option class for all zones. In this
     constructor, the input configuration file is parsed and all options are
     read and stored. ---*/
    
    config_container[iZone] = new CConfig(config_file_name, SU2_DEF, iZone, nZone, 0, VERB_HIGH);
        
    /*--- Definition of the geometry class to store the primal grid in the partitioning process. ---*/
    
    CGeometry *geometry_aux = NULL;
    
    /*--- All ranks process the grid and call ParMETIS for partitioning ---*/
    
    geometry_aux = new CPhysicalGeometry(config_container[iZone], iZone, nZone);
    
    /*--- Color the initial grid and set the send-receive domains (ParMETIS) ---*/
    
    geometry_aux->SetColorGrid_Parallel(config_container[iZone]);
    
    /*--- Allocate the memory of the current domain, and
     divide the grid between the nodes ---*/
    
    geometry_container[iZone] = new CPhysicalGeometry(geometry_aux, config_container[iZone], 1);
    
    /*--- Deallocate the memory of geometry_aux ---*/
    
    delete geometry_aux;

    /*--- Add the Send/Receive boundaries ---*/
    
    geometry_container[iZone]->SetSendReceive(config_container[iZone]);
    
    /*--- Add the Send/Receive boundaries ---*/
    
    geometry_container[iZone]->SetBoundaries(config_container[iZone]);
    
  }
  
  /*--- Set up a timer for performance benchmarking (preprocessing time is included) ---*/
  
#ifdef HAVE_MPI
  StartTime = MPI_Wtime();
#else
  StartTime = double(clock())/double(CLOCKS_PER_SEC);
#endif
  
  /*--- Computational grid preprocesing ---*/
  
  if (rank == MASTER_NODE) cout << endl << "----------------------- Preprocessing computations ----------------------" << endl;
  
  /*--- Compute elements surrounding points, points surrounding points ---*/
  
  if (rank == MASTER_NODE) cout << "Setting local point connectivity." <<endl;
  geometry_container[ZONE_0]->SetPoint_Connectivity();
  
  /*--- Check the orientation before computing geometrical quantities ---*/
  
  if (rank == MASTER_NODE) cout << "Checking the numerical grid orientation of the interior elements." <<endl;
  geometry_container[ZONE_0]->Check_IntElem_Orientation(config_container[ZONE_0]);

  /*--- Create the edge structure ---*/
  
  if (rank == MASTER_NODE) cout << "Identify edges and vertices." <<endl;
  geometry_container[ZONE_0]->SetEdges(); geometry_container[ZONE_0]->SetVertex(config_container[ZONE_0]);
  
  /*--- Compute center of gravity ---*/
  
  if (rank == MASTER_NODE) cout << "Computing centers of gravity." << endl;
  geometry_container[ZONE_0]->SetCG();
  
  /*--- Create the dual control volume structures ---*/
  
  if (rank == MASTER_NODE) cout << "Setting the bound control volume structure." << endl;
  geometry_container[ZONE_0]->SetBoundControlVolume(config_container[ZONE_0], ALLOCATE);
  
  /*--- Output original grid for visualization, if requested (surface and volumetric) ---*/
  
  if (config_container[ZONE_0]->GetVisualize_Deformation()) {

    output->SetMesh_Files(geometry_container, config_container, SINGLE_ZONE, true, false);

//    if (rank == MASTER_NODE) cout << "Writing an STL file of the surface mesh." << endl;
//    if (size > 1) sprintf (buffer_char, "_%d.stl", rank+1); else sprintf (buffer_char, ".stl");
//    strcpy (out_file, "Surface_Grid"); strcat(out_file, buffer_char); geometry[ZONE_0]->SetBoundSTL(out_file, true, config[ZONE_0]);
    
  }
  
  /*--- Surface grid deformation using design variables ---*/
  
  if (rank == MASTER_NODE) cout << endl << "------------------------- Surface grid deformation ----------------------" << endl;
  
  /*--- Definition and initialization of the surface deformation class ---*/
  
  surface_movement = new CSurfaceMovement();
  
  /*--- Copy coordinates to the surface structure ---*/

  surface_movement->CopyBoundary(geometry_container[ZONE_0], config_container[ZONE_0]);
  
  /*--- Surface grid deformation ---*/
  
  if (rank == MASTER_NODE) cout << "Performing the deformation of the surface grid." << endl;
  surface_movement->SetSurface_Deformation(geometry_container[ZONE_0], config_container[ZONE_0]);
  
  /*--- Volumetric grid deformation ---*/
  
  if (config_container[ZONE_0]->GetDesign_Variable(0) != FFD_SETTING) {
    
    if (rank == MASTER_NODE) cout << endl << "----------------------- Volumetric grid deformation ---------------------" << endl;
    
    /*--- Definition of the Class for grid movement ---*/
    
    grid_movement = new CVolumetricMovement(geometry_container[ZONE_0]);
    
    if (rank == MASTER_NODE) cout << "Performing the deformation of the volumetric grid." << endl;
    
    grid_movement->SetVolume_Deformation(geometry_container[ZONE_0], config_container[ZONE_0], false);
    
  }
  
  /*--- Computational grid preprocesing ---*/
  
  if (rank == MASTER_NODE) cout << endl << "----------------------- Write deformed grid files -----------------------" << endl;
  
  /*--- Output deformed grid for visualization, if requested (surface and volumetric), in parallel 
   requires to move all the data to the master node---*/
  
  output = new COutput();
  
  output->SetMesh_Files(geometry_container, config_container, SINGLE_ZONE, false, true);
  
  /*--- Write the the free-form deformation boxes after deformation. ---*/

  if (rank == MASTER_NODE) cout << "Adding FFD information to the SU2 file." << endl;
    
  surface_movement->WriteFFDInfo(geometry_container[ZONE_0], config_container[ZONE_0]);
  
  /*--- Synchronization point after a single solver iteration. Compute the
   wall clock time required. ---*/
  
#ifdef HAVE_MPI
  StopTime = MPI_Wtime();
#else
  StopTime = double(clock())/double(CLOCKS_PER_SEC);
#endif
  
  /*--- Compute/print the total time for performance benchmarking. ---*/
  
  UsedTime = StopTime-StartTime;
  if (rank == MASTER_NODE) {
    cout << "\nCompleted in " << fixed << UsedTime << " seconds on "<< size;
    if (size == 1) cout << " core." << endl; else cout << " cores." << endl;
  }
  
  /*--- Exit the solver cleanly ---*/
  
  if (rank == MASTER_NODE)
  cout << endl << "------------------------- Exit Success (SU2_DEF) ------------------------" << endl << endl;

  /*--- Finalize MPI parallelization ---*/

#ifdef HAVE_MPI
  MPI_Finalize();
#endif
  
  return EXIT_SUCCESS;
  
}
