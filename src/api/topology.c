#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ROOT-Sim.h>
#include <core.h>



unsigned int GetDirectionCount(int topology) {

	switch(topology) {

		case TOPOLOGY_HEXAGON:
			return 6;
		case TOPOLOGY_SQUARE:
			return 4;
		case TOPOLOGY_MESH:
		case TOPOLOGY_STAR:
			return pdes_config.nprocesses;
		case TOPOLOGY_BIDRING:
			return 2;
		case TOPOLOGY_RING:
			return 1;
		default:
			rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

	return UNDEFINED_LP;
}


unsigned int GetNeighbourFromDirection(int topology, int direction) {
	// receiver is not unsigned, because we exploit -1 as a border case in the bidring topology.
	int receiver;

 	// These must be unsigned. They are not checked for negative (wrong) values,
 	// but they would overflow, and are caught by a different check.
 	unsigned int edge;
 	unsigned int x = 0, y = 0, nx = 0, ny = 0;


	switch(topology) {

		case TOPOLOGY_HEXAGON:

			#define NW	0
			#define W	1
			#define SW	2
			#define SE	3
			#define E	4
			#define NE	5

			// Convert linear coords to hexagonal coords
			edge = sqrt(pdes_config.nprocesses);
			x = current_lp % edge;
			y = current_lp / edge;

			// Sanity check!
			if(edge * edge != pdes_config.nprocesses) {
				rootsim_error(true, "Hexagonal map wrongly specified!\n");
				return 0;
			}

			// Very simple case!
			if(pdes_config.nprocesses == 1) {
				receiver = current_lp;
				break;
			}


			// Select a random neighbour once, then move counter clockwise
			receiver = direction%6;
			bool invalid = false;

			// Find a random neighbour
			do {
				if(invalid) {
					receiver = (receiver + 1) % 6;
				}

				switch(receiver) {
					case NW:
						nx = (y % 2 == 0 ? x - 1 : x);
						ny = y - 1;
						break;
					case NE:
						nx = (y % 2 == 0 ? x : x + 1);
						ny = y - 1;
						break;
					case SW:
						nx = (y % 2 == 0 ? x - 1 : x);
						ny = y + 1;
						break;
					case SE:
						nx = (y % 2 == 0 ? x : x + 1);
						ny = y + 1;
						break;
					case E:
						nx = x + 1;
						ny = y;
						break;
					case W:
						nx = x - 1;
						ny = y;
						break;
					default:
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

				invalid = true;

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			receiver = (ny * edge + nx);

			#undef NE
			#undef NW
			#undef W
			#undef SW
			#undef SE
			#undef E

			break;


		case TOPOLOGY_SQUARE:

			#define N	0
			#define W	1
			#define S	2
			#define E	3

			// Convert linear coords to square coords
			edge = sqrt(pdes_config.nprocesses);
			x = current_lp % edge;
			y = current_lp / edge;


			// Sanity check!
			if(edge * edge != pdes_config.nprocesses) {
				rootsim_error(true, "Square map wrongly specified!\n");
				return 0;
			}


			// Very simple case!
			if(pdes_config.nprocesses == 1) {
				receiver = current_lp;
				break;
			}

			receiver = direction%4;
			if (receiver == 4) {
				receiver = 3;
			}
			invalid =false;

			// Find a random neighbour
			do {

				if(invalid) {
					receiver = (receiver + 1) % 4;
				}
				

				switch(receiver) {
					case N:
						nx = x;
						ny = y - 1;
						break;
					case S:
						nx = x;
						ny = y + 1;
						break;
					case E:
						nx = x + 1;
						ny = y;
						break;
					case W:
						nx = x - 1;
						ny = y;
						break;
					default:
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

				invalid = true;
				
				
			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			receiver = (ny * edge + nx);

			#undef N
			#undef W
			#undef S
			#undef E

			break;
		default:
			receiver =-1;
			rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

	return (unsigned int)receiver;
}

unsigned int FindReceiver(int topology) {

	// receiver is not unsigned, because we exploit -1 as a border case in the bidring topology.
	int receiver;
 	double u;


 	// These must be unsigned. They are not checked for negative (wrong) values,
 	// but they would overflow, and are caught by a different check.
 	unsigned int edge;
 	unsigned int x = 0, y = 0, nx = 0, ny = 0;

	switch(topology) {

		case TOPOLOGY_HEXAGON:

			#define NW	0
			#define W	1
			#define SW	2
			#define SE	3
			#define E	4
			#define NE	5

			// Convert linear coords to hexagonal coords
			edge = sqrt(pdes_config.nprocesses);
			x = current_lp % edge;
			y = current_lp / edge;

			// Sanity check!
			if(edge * edge != pdes_config.nprocesses) {
				rootsim_error(true, "Hexagonal map wrongly specified!\n");
				return 0;
			}

			// Very simple case!
			if(pdes_config.nprocesses == 1) {
				receiver = current_lp;
				break;
			}

			// Select a random neighbour once, then move counter clockwise
			receiver = 6 * Random();
			bool invalid = false;

			// Find a random neighbour
			do {
				if(invalid) {
					receiver = (receiver + 1) % 6;
				}

				switch(receiver) {
					case NW:
						nx = (y % 2 == 0 ? x - 1 : x);
						ny = y - 1;
						break;
					case NE:
						nx = (y % 2 == 0 ? x : x + 1);
						ny = y - 1;
						break;
					case SW:
						nx = (y % 2 == 0 ? x - 1 : x);
						ny = y + 1;
						break;
					case SE:
						nx = (y % 2 == 0 ? x : x + 1);
						ny = y + 1;
						break;
					case E:
						nx = x + 1;
						ny = y;
						break;
					case W:
						nx = x - 1;
						ny = y;
						break;
					default:
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

				invalid = true;

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			receiver = (ny * edge + nx);

			#undef NE
			#undef NW
			#undef W
			#undef SW
			#undef SE
			#undef E

			break;


		case TOPOLOGY_SQUARE:

			#define N	0
			#define W	1
			#define S	2
			#define E	3

			// Convert linear coords to square coords
			edge = sqrt(pdes_config.nprocesses);
			x = current_lp % edge;
			y = current_lp / edge;

			// Sanity check!
			if(edge * edge != pdes_config.nprocesses) {
				rootsim_error(true, "Square map wrongly specified!\n");
				return 0;
			}


			// Very simple case!
			if(pdes_config.nprocesses == 1) {
				receiver = current_lp;
				break;
			}

			// Find a random neighbour
			do {

				receiver = 4 * Random();
				if(receiver == 4) {
					receiver = 3;
				}

				switch(receiver) {
					case N:
						nx = x;
						ny = y - 1;
						break;
					case S:
						nx = x;
						ny = y + 1;
						break;
					case E:
						nx = x + 1;
						ny = y;
						break;
					case W:
						nx = x - 1;
						ny = y;
						break;
					default:
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			receiver = (ny * edge + nx);

			#undef N
			#undef W
			#undef S
			#undef E

			break;



		case TOPOLOGY_MESH:

			receiver = (int)(pdes_config.nprocesses * Random());
			break;



		case TOPOLOGY_BIDRING:

			u = Random();

			if (u < 0.5) {
				receiver = current_lp - 1;
			} else {
				receiver= current_lp + 1;
			}

   			if (receiver == -1) {
				receiver = pdes_config.nprocesses - 1;
			}

			// Can't be negative from now on
			if ((unsigned int)receiver == pdes_config.nprocesses) {
				receiver = 0;
			}

			break;



		case TOPOLOGY_RING:

			receiver= current_lp + 1;

			if ((unsigned int)receiver == pdes_config.nprocesses) {
				receiver = 0;
			}

			break;


		case TOPOLOGY_STAR:

			if (current_lp == 0) {
				receiver = (int)(pdes_config.nprocesses * Random());
			} else {
				receiver = 0;
			}

			break;

		default:
			receiver =-1;
			rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

	return (unsigned int)receiver;

}
