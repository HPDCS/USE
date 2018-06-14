#include <stdlib.h>
#include <stdio.h>

#include <math.h>

#include "application.h"

int **edges;

int __FindReceiver(unsigned int sender , void * addr)
{
	// sender is the process' gid

	extern int **edges; /* [i][0] say how much nodes are connected to the i-th */
	unsigned int receiver, temp;
 	double u;

	lp_state_type *pcs;
        pcs = (lp_state_type*) addr;

	switch (TOPOLOGY) {
		case EXAGON:
			receiver = ((int) ((pcs->buff_topology->num_adjacent ) * Random())) ;
			receiver = pcs->buff_topology->adjacent_identities[receiver];
			break;
		case POINT_TO_POINT:
			receiver=((int) ( n_prc_tot * Random())) ;
			/* hot spot update*/
			break;
		case BID_RING:
			u= Random();
			if (( u < 0.5 ) && (u >= 0.0 ) ) {
				receiver= sender - 1;
			} else {
				if (( u <= 1.0 ) && (u >= 0.5 ) )
					receiver= sender + 1;
				else receiver= sender;
			}
   			if (receiver==-1)
				receiver=n_prc_tot-1;
			else
				if (receiver==n_prc_tot)
					receiver= 0;
		break;

		case UNI_RING:
			u= Random();
			if (u>=0.0)
				receiver= sender + 1;
			else receiver= sender;
			if (receiver==n_prc_tot)
				receiver=0;
		break;


		case HYPER:


			temp= ((int) ( (edges[sender][0]) * Random())) ;

			if (temp==(edges[sender][0]+1)) {
				//fprintf(fout, "ERRORE nell'indirizzamento hyper: randomly extracted index =%d\n", temp);
				//fprintf(fout, "assegnero' edges[%d][%d] quando al max si arriva a %d\n\n", sender, temp+1, sender, edges[sender][0] );
				//fprintf(fout, "assegnero' edges[%d][%d] quando al max si arriva a %d\n\n", sender, temp+1, sender);
				//fclose( fout );
				abort();
			}


			receiver= edges[sender][temp+1];
		break;

		case STAR:
			if (!sender)
				receiver=((int) ( n_prc_tot * Random())) ;
			else
				receiver= 0;
		break;


		case MESH:



			temp= ((int) ( (edges[sender][0]) * Random())) ;

			if (temp==(edges[sender][0]+1)) {
				//fprintf(fout, "ERRORE nell'indirizzamento mesh: indice estrattto casualmente =%d\n", temp);
				//fprintf(fout, "assegnero' edges[%d][%d] quando al max si arriva a %d\n\n", sender, temp+1, sender, edges[sender][0] );
				//fprintf(fout, "assegnero' edges[%d][%d] quando al max si arriva a %d\n\n", sender, temp+1, sender);
				//fclose( fout );
				abort();
			}

			receiver= edges[sender][temp+1];



		break;


		default:
			receiver = ((int) (n_prc_tot * Random())) ;

	}

	return receiver;

}

void set_my_topology(unsigned int gid, _PCS_routing *buffer) {
	// Defines the information on the topology associated with the LP identified by gid.
	// Routing information is copied into the routing buffer.
	int t = gid;

	if (TOPOLOGY != EXAGON) return; 

	if ((D*D)!=n_prc_tot) {
		printf("The hexagonal map is not defined, check processi_totli and D\n");
		exit(-1);
	}


	int pp,k,i;
	buffer->num_adjacent = 0;

	for (pp=0; pp<6 ; pp++) buffer->adjacent_identities[pp] = -1;


	// k is the lid of the process having t as gid
	k = t % D;
	// i it the id of the kernel hosting the process having t as gid
	i = (int)((t - k)/D);


	if(i == 0){ // master

		if (k == 0){
			buffer->num_adjacent = 2;
			buffer->adjacent_identities[0] = t+1;
			buffer->adjacent_identities[1] = D;

		}

		if (k == (D-1)){
			buffer->num_adjacent = 3;
			buffer->adjacent_identities[0] = t-1;
			buffer->adjacent_identities[1] = 2*D - 2;
			buffer->adjacent_identities[2] = 2*D - 1;

		}

		if((k>0) && (k<(D-1))){
			buffer->num_adjacent = 4;
			buffer->adjacent_identities[0] = t-1;
			buffer->adjacent_identities[1] = t + D - 1;
			buffer->adjacent_identities[2] = t + D ;
			buffer->adjacent_identities[3] = t + 1;

		}
	}

	if( i == (D-1)) {
		if (k == 0 ){

			if( (i%2) != 0 ){
				buffer->num_adjacent = 3;
				buffer->adjacent_identities[0] = t-D;
				buffer->adjacent_identities[1] = t - D + 1;
				buffer->adjacent_identities[2] = t + 1;

			}
			else{
				buffer->num_adjacent = 2;
				buffer->adjacent_identities[0] = t-D;
				buffer->adjacent_identities[1] = t+1;
			}

		}

		if (k == (D-1)) {

			if( (i%2) != 0 ) {
				buffer->num_adjacent = 2;
				buffer->adjacent_identities[0] = t-D;
				buffer->adjacent_identities[1] = t- 1;
			}
			else{
				buffer->num_adjacent = 3;
				buffer->adjacent_identities[0] = t-D;
				buffer->adjacent_identities[1] = t-D-1;
				buffer->adjacent_identities[2] = t-1;
			}
		}

		if((k>0) && (k<(D-1))){

			buffer->num_adjacent = 4;
			buffer->adjacent_identities[0] = t-1;
			buffer->adjacent_identities[1] = t - D ;
			buffer->adjacent_identities[2] = t - D + 1;
			buffer->adjacent_identities[3] = t + 1;

		}
	}


	if ( (i > 0) && (i < (D-1)) ){

		if( k == 0){
			if((i%2)!=0){
				buffer->num_adjacent = 5;
				buffer->adjacent_identities[0] = (i-1)*D + k  ;
				buffer->adjacent_identities[1] = (i-1)*D + k +1 ;
				buffer->adjacent_identities[2] = t + 1;
				buffer->adjacent_identities[3] = (i+1)*D + k +1 ;
				buffer->adjacent_identities[4] = (i+1)*D + k ;
			}
			else{
				buffer->num_adjacent = 3;
				buffer->adjacent_identities[0] = (i-1)*D + k  ;
				buffer->adjacent_identities[1] = t + 1;
				buffer->adjacent_identities[2] = (i+1)*D + k  ;
			}
		}

		if(k == (D-1)){

			if((i%2)==0){
				buffer->num_adjacent = 5;
				buffer->adjacent_identities[0] = (i-1)*D + k  ;
				buffer->adjacent_identities[1] = (i-1)*D + k -1;
				buffer->adjacent_identities[2] = t - 1 ;
				buffer->adjacent_identities[3] = (i+1)*D + k -1 ;
				buffer->adjacent_identities[4] = (i+1)*D + k ;
			}
			else{
				buffer->num_adjacent = 3;
				buffer->adjacent_identities[0] = (i-1)*D + k  ;
				buffer->adjacent_identities[1] = t - 1;
				buffer->adjacent_identities[2] = (i+1)*D + k  ;
			}
		}


		if((k>0) && (k<D-1)){

			if((i%2)!=0){
				buffer->num_adjacent = 6;
				buffer->adjacent_identities[0] = t-1;
				buffer->adjacent_identities[1] = (i-1)*D + k  ;
				buffer->adjacent_identities[2] = (i-1)*D + k +1 ;
				buffer->adjacent_identities[3] = t + 1;
				buffer->adjacent_identities[4] = (i+1)*D + k +1 ;
				buffer->adjacent_identities[5] = (i+1)*D + k;
			} else {
				buffer->num_adjacent = 6;
				buffer->adjacent_identities[0] = t-1;
				buffer->adjacent_identities[1] = (i-1)*D + k  -1 ;
				buffer->adjacent_identities[2] = (i-1)*D + k  ;
				buffer->adjacent_identities[3] = t + 1;
				buffer->adjacent_identities[4] = (i+1)*D + k  ;
				buffer->adjacent_identities[5] = (i+1)*D + k -1;
			}
		}

	}

/*
	printf("(LP %d) Adjacences:\n",gid);
	for (pp=0; pp<buffer->num_adjacent; pp++)  {
		printf("%d - ", buffer->adjacent_identities[pp]);
	}

	printf("\n");
	fflush(stdout);
*/
}

