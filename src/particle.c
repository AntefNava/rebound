/**
 * @file 	particle.c
 * @brief 	reb_particle structure and main particle routines.
 * @author 	Hanno Rein <hanno@hanno-rein.de>
 * 
 * @section 	LICENSE
 * Copyright (c) 2011 Hanno Rein, Shangfei Liu
 *
 * This file is part of rebound.
 *
 * rebound is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rebound is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rebound.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "rebound.h"
#include "tree.h"
#include "boundary.h"
#ifndef COLLISIONS_NONE
#include "collision.h"
#endif // COLLISIONS_NONE
#ifdef MPI
#include "communication_mpi.h"
#endif // MPI

#ifdef GRAVITY_GRAPE
#warning Fix this. 
extern double gravity_minimum_mass;
#endif // GRAVITY_GRAPE

static void reb_add_local(struct reb_simulation* const r, struct reb_particle pt){
	if (reb_boundary_particle_is_in_box(r, pt)==0){
		// reb_particle has left the box. Do not add.
		reb_warning("Did not add particle outside of box boundaries.");
		return;
	}
	while (r->allocatedN<=r->N){
		r->allocatedN += 128;
		r->particles = realloc(r->particles,sizeof(struct reb_particle)*r->allocatedN);
	}

	r->particles[r->N] = pt;
	r->particles[r->N].ap = NULL;
	r->particles[r->N].sim = r;
	if (r->gravity==REB_GRAVITY_TREE || r->collision==REB_COLLISION_TREE){
		reb_tree_add_particle_to_tree(r, r->N);
	}
	(r->N)++;
}

void reb_add(struct reb_simulation* const r, struct reb_particle pt){
	if (r->N_var){
		reb_warning("Trying to add particle after calling megno_init().");
	}
#ifndef COLLISIONS_NONE
	if (pt.r>=r->max_radius[0]){
		r->max_radius[1] = r->max_radius[0];
		r->max_radius[0] = pt.r;
	}else{
		if (pt.r>=r->max_radius[1]){
			r->max_radius[1] = pt.r;
		}
	}
#endif 	// COLLISIONS_NONE
#ifdef GRAVITY_GRAPE
	if (pt.m<gravity_minimum_mass){
		gravity_minimum_mass = pt.m;
	}
#endif // GRAVITY_GRAPE
#ifdef MPI
	int rootbox = reb_get_rootbox_for_particle(r, pt);
	int root_n_per_node = root_n/mpi_num;
	int proc_id = rootbox/root_n_per_node;
	if (proc_id != mpi_id && r->N >= r->N_active){
		// Add particle to array and send them to proc_id later. 
		communication_mpi_add_particle_to_send_queue(pt,proc_id);
		return;
	}
#endif // MPI
	// Add particle to local partical array.
	reb_add_local(r, pt);
}

int reb_get_rootbox_for_particle(const struct reb_simulation* const r, struct reb_particle pt){
	if (r->root_size==-1) return 0;
	int i = ((int)floor((pt.x + r->boxsize.x/2.)/r->root_size)+r->root_nx)%r->root_nx;
	int j = ((int)floor((pt.y + r->boxsize.y/2.)/r->root_size)+r->root_ny)%r->root_ny;
	int k = ((int)floor((pt.z + r->boxsize.z/2.)/r->root_size)+r->root_nz)%r->root_nz;
	int index = (k*r->root_ny+j)*r->root_nx+i;
	return index;
}

void reb_remove_all(struct reb_simulation* const r){
	r->N 		= 0;
	r->allocatedN 	= 0;
	r->N_active 	= -1;
	r->N_var 	= 0;
	free(r->particles);
	r->particles 	= NULL;
}

int reb_remove(struct reb_simulation* const r, int index, int keepSorted){
	if (r->N==1){
		fprintf(stderr, "Last particle removed.\n");
		return 1;
	}
	if (index >= r->N){
		fprintf(stderr, "\nIndex %d passed to particles_remove was out of range (N=%d).  Did not remove particle.\n", index, r->N);
		return 0;
	}
	if (r->N_var){
		fprintf(stderr, "\nRemoving particles not supported when calculating MEGNO.  Did not remove particle.\n");
		return 0;
	}
	(r->N)--;
	if(keepSorted){
		for(int j=index; j<r->N; j++){
			r->particles[j] = r->particles[j+1];
		}
	}
	else{
		r->particles[index] = r->particles[r->N];
	}

	return 1;
}

int reb_remove_by_id(struct reb_simulation* const r, int id, int keepSorted){
	int success = 0;
	for(int i=0;i<r->N;i++){
		if(r->particles[i].id == id){
			success = reb_remove(r, i, keepSorted);
			break;
		}
	}

	if(!success){
		fprintf(stderr, "\nIndex passed to particles_remove_id (id = %d) not found in particles array.  Did not remove particle.\n", id);
	}
	return success;
}
