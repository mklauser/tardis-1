#include "cmontecarlo.h"

inline tardis_error_t line_search(double *nu, double nu_insert, int64_t number_of_lines, int64_t *result)
{
  tardis_error_t ret_val = TARDIS_ERROR_OK;
  int64_t imin, imax;
  imin = 0;
  imax = number_of_lines - 1;
  if (nu_insert > nu[imin])
    {
      *result = imin;
    }
  else if (nu_insert < nu[imax])
    {
      *result = imax + 1;
    }
  else
    {
      ret_val = binary_search(nu, nu_insert, imin, imax, result);
      *result = *result + 1;
    }
  return ret_val;
}

inline tardis_error_t binary_search(double *x, double x_insert, int64_t imin, int64_t imax, int64_t *result)
{
  /*
    Have in mind that *x points to a reverse sorted array.
    That is large values will have small indices and small ones
    will have large indices.
  */
  tardis_error_t ret_val = TARDIS_ERROR_OK;
  if (x_insert > x[imin] || x_insert < x[imax])
    {
      ret_val = TARDIS_ERROR_BOUNDS_ERROR;
    }
  else
    {
      int imid = (imin + imax) / 2;
      while (imax - imin > 2)
	{
	  if (x[imid] < x_insert)
	    {
	      imax = imid + 1;
	    }
	  else
	    {
	      imin = imid;
	    }
	  imid = (imin + imax) / 2;
	}
      if (imax - imid == 2 && x_insert < x[imin + 1])
	{
	  *result = imin + 1;
	}
      else
	{
	  *result = imin;
	}
    }
  return ret_val;
}



inline void check_array_bounds(int64_t ioned,int64_t nrow,int64_t ncolums)
{
    if (ioned > ((ncolums + 1) * (nrow +1)))
        {
        fprintf(stderr, "Array Index Out Of Bounds");
        exit(1);
        }
}

inline void set_array_int(int64_t irow,int64_t icolums,int64_t nrow,int64_t ncolums , int64_t *array, int64_t val )
    {
    int64_t ioned = 0;
    ioned = nrow * icolums + irow;
    check_array_bounds(ioned, nrow, ncolums);
    array[ioned] = val;
    }

inline void set_array_double(int64_t  irow, int64_t icolums, int64_t nrow, int64_t ncolums , double *array, double val )
    {
    int64_t ioned = 0;
    ioned = nrow * icolums + irow;
    check_array_bounds(ioned, nrow, ncolums);
    array[ioned] = val;
    }

inline int64_t get_array_int( int64_t irow, int64_t icolums, int64_t nrow, int64_t ncolums , int64_t *array)
    {
    int64_t ioned = 0;
    ioned = nrow * icolums + irow;
    check_array_bounds(ioned, nrow, ncolums);
    return array[ioned];
    }

inline double get_array_double( int64_t irow, int64_t icolums, int64_t nrow, int64_t ncolums , double *array)
    {
    int64_t ioned = 0;
    ioned = nrow * icolums + irow;
    check_array_bounds(ioned, nrow, ncolums);
    return array[ioned];
    }

inline void create_kpacket(rpacket *packet)
{
    double comov_energy, comov_nu, doppler_factor;
    doppler_factor = rpacket_doppler_factor(packet, storage);
    comov_energy = rpacket_get_energy(packet) * doppler_factor;
    comov_nu = rpacket_get_nu(packet) * doppler_factor;
    rpacket_set_comov_energy(packet,comov_energy);
    rpacket_set_comov_nu(packet, comov_nu);
    packet->packet_status = TARDIS_K_PACKET_IN_PROCESS;


//ToDo: compute comiving E, mu and, nu and set these values
}

inline void create_rpacket(rpacket *packet, double new_comov_nu, double new_comov_mu)
{
    double energy, nu, inverse_doppler_factor;
    inverse_doppler_factor = 1.0 / (1.0 - new_comov_mu * rpacket_get_r(packet) *
    storage->inverse_time_explosion * INVERSE_C);
    energy = rpacket_get_comov_energy(packet) * inverse_doppler_factor;
    nu = new_comov_nu * inverse_doppler_factor;
    rpacket_set_energy(packet, energy);
    rpacket_set_nu(packet, nu);
    rpacket_set_mu(packet, new_comov_mu);
    packet->packet_status = TARDIS_R_PACKET_IN_PROCESS;

//ToDo: get restframe mu and nu compute comoving
}


inline double rpacket_doppler_factor(rpacket_t *packet, storage_model_t *storage)
{
  return 1.0 - rpacket_get_mu(packet) * rpacket_get_r(packet) * storage->inverse_time_explosion * INVERSE_C;
}

inline double compute_distance2boundary(rpacket_t *packet, storage_model_t *storage)
{
  double r = rpacket_get_r(packet);
  double mu = rpacket_get_mu(packet);  
  double r_outer = storage->r_outer[rpacket_get_current_shell_id(packet)];
  double r_inner = storage->r_inner[rpacket_get_current_shell_id(packet)];
  double d_outer = sqrt(r_outer * r_outer + ((mu * mu - 1.0) * r * r)) - (r * mu);
  double d_inner;
  double result;
  if (rpacket_get_recently_crossed_boundary(packet) == 1)
    {
      rpacket_set_next_shell_id(packet, 1);
      return d_outer;
    }
  else
    {
      double check = r_inner * r_inner + (r * r * (mu * mu - 1.0));
      if (check < 0.0) 
	{
	  rpacket_set_next_shell_id(packet, 1);
	  return d_outer;
	}
      else
	{
	  d_inner =  mu < 0.0 ? -r * mu - sqrt(check) : MISS_DISTANCE;
	}
    }
  if (d_inner < d_outer)
    {
      rpacket_set_next_shell_id(packet, -1);
      return d_inner;
    }
  else
    {
      rpacket_set_next_shell_id(packet, 1);
      return d_outer;
    }
}

inline tardis_error_t compute_distance2line(rpacket_t *packet, storage_model_t *storage, double *result)
{
  tardis_error_t ret_val = TARDIS_ERROR_OK;
  if (rpacket_get_last_line(packet))
    {
      *result = MISS_DISTANCE;
    }
  else
    {
      double r = rpacket_get_r(packet);
      double mu = rpacket_get_mu(packet);
      double nu = rpacket_get_nu(packet);
      double nu_line = rpacket_get_nu_line(packet);
      double t_exp = storage->time_explosion;
      double inverse_t_exp = storage->inverse_time_explosion;
      int64_t cur_zone_id = rpacket_get_current_shell_id(packet);
      double comov_nu, doppler_factor;
      doppler_factor = 1.0 - mu * r * inverse_t_exp * INVERSE_C;
      comov_nu = nu * doppler_factor;
      if (comov_nu < nu_line)
	{
	  fprintf(stderr, "ERROR: Comoving nu less than nu_line!\n");
	  fprintf(stderr, "comov_nu = %f\n", comov_nu);
	  fprintf(stderr, "nu_line = %f\n", nu_line);
	  fprintf(stderr, "(comov_nu - nu_line) / nu_line = %f\n", (comov_nu - nu_line) / nu_line);
	  fprintf(stderr, "r = %f\n", r);
	  fprintf(stderr, "mu = %f\n", mu);
	  fprintf(stderr, "nu = %f\n", nu);
	  fprintf(stderr, "doppler_factor = %f\n", doppler_factor);
	  fprintf(stderr, "cur_zone_id = %d\n", cur_zone_id);
	  ret_val = TARDIS_ERROR_COMOV_NU_LESS_THAN_NU_LINE;
	}
      else
	{
	  *result = ((comov_nu - nu_line) / nu) * C * t_exp;
	}
    }
  return ret_val;
}

inline double compute_distance2electron(rpacket_t *packet, storage_model_t *storage)
{
  if (rpacket_get_virtual_packet(packet) > 0)
    {
      return MISS_DISTANCE;
    }
  double inverse_ne = storage->inverse_electron_densities[rpacket_get_current_shell_id(packet)] * 
    storage->inverse_sigma_thomson;
  return rpacket_get_tau_event(packet) * inverse_ne;
}

inline void compute_distance2continuum(rpacket_t *packet, storage_model_t *storage)
{
    double chi_boundfree, chi_freefree, chi_electron, chi_continuum, d_continuum;
    if (packet->virtual_packet > 0)
        {
        //Set all continuum distances to MISS_DISTANCE in case of an virtual_packet
        rpacket_set_d_continuum(packet, MISS_DISTANCE);
        rpacket_set_chi_boundfree(packet,0.0);
        rpacket_set_chi_electron(packet,0.0);
        rpacket_set_chi_freefree(packet,0.0);
        rpacket_set_chi_continuum(packet,0.0);
        }
        else
        {
        // Compute the continuum oddities for a real packet
        chi_boundfree = 0.0;//calculate_chi_bf(packet, storage);
        chi_boundfree = calculate_chi_bf(packet, storage); // For Debug;
        chi_freefree = 0.0;
        chi_electron =  storage->electron_densities[packet->current_shell_id] * storage->sigma_thomson; // For Debugging set * to /
        chi_continuum = chi_boundfree + chi_freefree + chi_electron;
        d_continuum = rpacket_get_tau_event(packet) / chi_continuum;

//        fprintf(stderr, "--------\n");
//        fprintf(stderr, "nu = %e \n", rpacket_get_nu(packet));
//        fprintf(stderr, "chi_electron = %e\n", chi_electron);
//        fprintf(stderr, "chi_boundfree = %e\n", calculate_chi_bf(packet, storage));
//        fprintf(stderr, "chi_line = %e \n", rpacket_get_tau_event(packet) / rpacket_get_d_line(packet));
//        fprintf(stderr, "--------\n");

        rpacket_set_chi_freefree(packet, chi_freefree);
        rpacket_set_chi_boundfree(packet, chi_boundfree);
        rpacket_set_chi_electron(packet, chi_electron);
        rpacket_set_chi_continuum(packet, chi_continuum);
        rpacket_set_d_continuum(packet, d_continuum);
    }
}

inline double calculate_chi_bf(rpacket_t *packet, storage_model_t *storage)
{
    double bf_helper = 0;
    double nu_th;
    double l_pop_r;
    double l_pop;
    double T;
//    double kB;
    double nu;
    int64_t i = 0;
    int64_t I;
    int64_t atom;
    int64_t ion;
    int64_t level;

    nu = rpacket_get_nu(packet);
    T = storage->t_electrons[packet->current_shell_id];
//    kB = storage->kB;
    I = storage->chi_bf_index_to_level_nrow; // This is equal to the number of levels

    for(i=0;i<=I;++i){
        nu_th = storage->bound_free_th_frequency[i];
        if (nu_th < nu){

            // get the levelpopulation for the level ijk in the current shell
            l_pop = get_array_double(i,packet->current_shell_id, storage->bf_level_population_nrow,
            		storage->bf_level_population_ncolum, storage->bf_level_population);

            //get the levelpopulation ratio \frac{n_{0,j+1,k}}{n_{i,j,k}} \frac{n_{i,j,k}}{n_{0,j+1,k}}^{*}
            l_pop_r = get_array_double(i,packet->current_shell_id, storage->bf_lpopulation_ratio_nlte_lte_nrow,
            		storage->bf_lpopulation_ratio_nlte_lte_ncolum, storage->bf_lpopulation_ratio_nlte_lte);

            bf_helper += l_pop * storage->bf_cross_sections[i] *  pow((nu_th/nu),3) * (1-(l_pop_r * exp(-(H * nu)/KB /T)));
//            fprintf(stderr, ">>> \n");
//            fprintf(stderr, "exp  = %e \n" ,exp(-(H * nu)/KB /T));
//            fprintf(stderr, "lpop = %e \n", l_pop);
//            fprintf(stderr, "lpop ratio = %e \n", l_pop_r);
//            fprintf(stderr, "<<< \n");
        }
    }
    return bf_helper;
}


inline int64_t macro_atom(rpacket_t *packet, storage_model_t *storage)
{
  int emit, i = 0;
  double p, event_random;
  int activate_level = storage->line2macro_level_upper[rpacket_get_next_line_id(packet) - 1];
  while (emit != -1)
    {
      event_random = rk_double(&mt_state);
      i = storage->macro_block_references[activate_level] - 1;
      p = 0.0;
      do
	{
	  p += storage->transition_probabilities[rpacket_get_current_shell_id(packet) * storage->transition_probabilities_nd + (++i)];
	} while (p <= event_random);
      emit = storage->transition_type[i];
      activate_level = storage->destination_level_id[i];
    }
  return storage->transition_line_id[i];
}

inline double move_packet(rpacket_t *packet, storage_model_t *storage, double distance)
{
  double new_r, doppler_factor, comov_energy, comov_nu;
  doppler_factor = rpacket_doppler_factor(packet, storage);
  if (distance > 0.0)
    {
      double r = rpacket_get_r(packet);
      new_r = sqrt(r * r + distance * distance + 2.0 * r * distance * rpacket_get_mu(packet));
      rpacket_set_mu(packet, (rpacket_get_mu(packet) * r + distance) / new_r);
      rpacket_set_r(packet, new_r);
      if (rpacket_get_virtual_packet(packet) <= 0)
	{
	  comov_energy = rpacket_get_energy(packet) * doppler_factor;
	  comov_nu = rpacket_get_nu(packet) * doppler_factor;
	  storage->js[rpacket_get_current_shell_id(packet)] += comov_energy * distance;
	  storage->nubars[rpacket_get_current_shell_id(packet)] += comov_energy * distance * comov_nu;
	}
    }
  return doppler_factor;
}

inline void increment_j_blue_estimator(rpacket_t *packet, storage_model_t *storage,
				       double d_line, int64_t j_blue_idx)
{
  double comov_energy, r_interaction, mu_interaction, doppler_factor;
  double r = rpacket_get_r(packet);
  r_interaction = sqrt(r * r + d_line * d_line + 2.0 * r * d_line * rpacket_get_mu(packet));
  mu_interaction = (rpacket_get_mu(packet) * r + d_line) / r_interaction;
  doppler_factor = 1.0 - mu_interaction * r_interaction * 
    storage->inverse_time_explosion * INVERSE_C;
  comov_energy = rpacket_get_energy(packet) * doppler_factor;
  storage->line_lists_j_blues[j_blue_idx] += comov_energy / rpacket_get_nu(packet);
}

int64_t montecarlo_one_packet(storage_model_t *storage, rpacket_t *packet, int64_t virtual_mode)
{
  int64_t i;
  rpacket_t virt_packet;
  double mu_bin;
  double mu_min;
  double doppler_factor_ratio;
  double weight; 
  int64_t virt_id_nu;
  if (virtual_mode == 0)
    {
      montecarlo_one_packet_loop(storage, packet, 0);
    }
  else
    {
      for (i = 0; i < rpacket_get_virtual_packet_flag(packet); i++)
	{
	  memcpy((void *)&virt_packet, (void *)packet, sizeof(rpacket_t));
	  if (virt_packet.r > storage->r_inner[0])
	    {
	      mu_min = -1.0 * sqrt(1.0 - (storage->r_inner[0] / virt_packet.r) * 
				   (storage->r_inner[0] / virt_packet.r));
	    }
	  else
	    {
	      mu_min = 0.0;
	    }
	  mu_bin = (1.0 - mu_min) / rpacket_get_virtual_packet_flag(packet);
	  virt_packet.mu = mu_min + (i + rk_double(&mt_state)) * mu_bin;
	  switch(virtual_mode)
	    {
	    case -2:
	      weight = 1.0 / rpacket_get_virtual_packet_flag(packet);
	      break;
	    case -1:
	      weight = 2.0 * virt_packet.mu / rpacket_get_virtual_packet_flag(packet);
	      break;
	    case 1:
	      weight = (1.0 - mu_min) / 2.0 / rpacket_get_virtual_packet_flag(packet);
	      break;
	    default:
	      fprintf(stderr, "Something has gone horribly wrong!\n");
	    }
	  doppler_factor_ratio = 
	    rpacket_doppler_factor(packet, storage) /
	    rpacket_doppler_factor(&virt_packet, storage);
	  virt_packet.energy = rpacket_get_energy(packet) * doppler_factor_ratio;
	  virt_packet.nu = rpacket_get_nu(packet) * doppler_factor_ratio;
	  montecarlo_one_packet_loop(storage, &virt_packet, 1);
	  if ((virt_packet.nu < storage->spectrum_end_nu) && 
	      (virt_packet.nu > storage->spectrum_start_nu))
	    {
	      virt_id_nu = floor((virt_packet.nu - storage->spectrum_start_nu) / 
				 storage->spectrum_delta_nu);
	      storage->spectrum_virt_nu[virt_id_nu] += virt_packet.energy * weight;
	    }
	}
    }
}

void move_packet_across_shell_boundary(rpacket_t *packet, storage_model_t *storage, double distance)
{
  double comov_energy, doppler_factor, comov_nu, inverse_doppler_factor, delta_tau_event;
  move_packet(packet, storage, distance);
  if (rpacket_get_virtual_packet(packet) > 0)
    {
      delta_tau_event = rpacket_get_chi_continuum(packet) * distance;
      rpacket_set_tau_event(packet, rpacket_get_tau_event(packet) + delta_tau_event);
    }
  else
    {
      rpacket_reset_tau_event(packet);
    }
  if ((rpacket_get_current_shell_id(packet) < storage->no_of_shells - 1 && rpacket_get_next_shell_id(packet) == 1) || 
      (rpacket_get_current_shell_id(packet) > 0 && rpacket_get_next_shell_id(packet) == -1))
    {
      rpacket_set_current_shell_id(packet, rpacket_get_current_shell_id(packet) + rpacket_get_next_shell_id(packet));
      rpacket_set_recently_crossed_boundary(packet, rpacket_get_next_shell_id(packet));
    }
  else if (rpacket_get_next_shell_id(packet) == 1)
    {
      rpacket_set_status(packet, TARDIS_PACKET_STATUS_EMITTED);
    }
  else if ((storage->reflective_inner_boundary == 0) || 
	   (rk_double(&mt_state) > storage->inner_boundary_albedo))
    {
      rpacket_set_status(packet, TARDIS_PACKET_STATUS_REABSORBED);
    }
  else
    {
      doppler_factor = rpacket_doppler_factor(packet, storage);
      comov_nu = rpacket_get_nu(packet) * doppler_factor;
      comov_energy = rpacket_get_energy(packet) * doppler_factor;
      rpacket_set_mu(packet, rk_double(&mt_state));
      inverse_doppler_factor = 1.0 / rpacket_doppler_factor(packet, storage);
      rpacket_set_nu(packet, comov_nu * inverse_doppler_factor);
      rpacket_set_energy(packet, comov_energy * inverse_doppler_factor);
      rpacket_set_recently_crossed_boundary(packet, 1);
      if (rpacket_get_virtual_packet_flag(packet) > 0)
	{
	  montecarlo_one_packet(storage, packet, -2);
	}
    }
}

void montecarlo_thomson_scatter(rpacket_t *packet, storage_model_t *storage, double distance)
{
  double comov_energy, doppler_factor, comov_nu, inverse_doppler_factor;
  doppler_factor = move_packet(packet, storage, distance);
  comov_nu = rpacket_get_nu(packet) * doppler_factor;
  comov_energy = rpacket_get_energy(packet) * doppler_factor;
  rpacket_set_mu(packet, 2.0 * rk_double(&mt_state) - 1.0);
  inverse_doppler_factor = 1.0 / rpacket_doppler_factor(packet, storage);
  rpacket_set_nu(packet, comov_nu * inverse_doppler_factor);
  rpacket_set_energy(packet, comov_energy * inverse_doppler_factor);
  rpacket_reset_tau_event(packet);
  rpacket_set_recently_crossed_boundary(packet, 0);
  storage->last_interaction_type[storage->current_packet_id] = 1;
  if (rpacket_get_virtual_packet_flag(packet) > 0)
    {
      montecarlo_one_packet(storage, packet, 1);
    }  
}

void montecarlo_line_scatter(rpacket_t *packet, storage_model_t *storage, double distance)
{
  double comov_energy = 0.0;
  int64_t emission_line_id = 0;
  double old_doppler_factor = 0.0;
  double inverse_doppler_factor = 0.0;
  double tau_line = 0.0;
  double tau_continuum = 0.0;
  double tau_combined = 0.0;
  bool virtual_close_line = false;
  int64_t j_blue_idx = -1;
  if (rpacket_get_virtual_packet(packet) == 0)
    {
      j_blue_idx = rpacket_get_current_shell_id(packet) * storage->line_lists_j_blues_nd + rpacket_get_next_line_id(packet);
      increment_j_blue_estimator(packet, storage, distance, j_blue_idx);
    }
  tau_line = storage->line_lists_tau_sobolevs[rpacket_get_current_shell_id(packet) * storage->line_lists_tau_sobolevs_nd + rpacket_get_next_line_id(packet)];
  tau_continuum = rpacket_get_chi_continuum(packet) * distance;
  tau_combined = tau_line + tau_continuum;
  rpacket_set_next_line_id(packet, rpacket_get_next_line_id(packet) + 1);
  if (rpacket_get_next_line_id(packet) == storage->no_of_lines)
    {
      rpacket_set_last_line(packet, true);
    }
  if (rpacket_get_virtual_packet(packet) > 0)
    {
      rpacket_set_tau_event(packet, rpacket_get_tau_event(packet) + tau_line);
    }
  else if (rpacket_get_tau_event(packet) < tau_combined)
    {
      old_doppler_factor = move_packet(packet, storage, distance);
      rpacket_set_mu(packet, 2.0 * rk_double(&mt_state) - 1.0);
      inverse_doppler_factor = 1.0 / rpacket_doppler_factor(packet, storage);
      comov_energy = rpacket_get_energy(packet) * old_doppler_factor;
      rpacket_set_energy(packet, comov_energy * inverse_doppler_factor);
      storage->last_line_interaction_in_id[storage->current_packet_id] = rpacket_get_next_line_id(packet) - 1;
      storage->last_line_interaction_shell_id[storage->current_packet_id] = rpacket_get_current_shell_id(packet);
      storage->last_interaction_type[storage->current_packet_id] = 2;
      if (storage->line_interaction_id == 0)
	{
	  emission_line_id = rpacket_get_next_line_id(packet) - 1;
	}
      else if (storage->line_interaction_id >= 1)
	{
	  emission_line_id = macro_atom(packet, storage);
	}
      storage->last_line_interaction_out_id[storage->current_packet_id] = emission_line_id;
      rpacket_set_nu(packet, storage->line_list_nu[emission_line_id] * inverse_doppler_factor);
      rpacket_set_nu_line(packet, storage->line_list_nu[emission_line_id]);
      rpacket_set_next_line_id(packet, emission_line_id + 1);
      rpacket_reset_tau_event(packet);
      rpacket_set_recently_crossed_boundary(packet, 0);
      if (rpacket_get_virtual_packet_flag(packet) > 0)
	{
	  virtual_close_line = false;
	  if (!rpacket_get_last_line(packet) &&
	      fabs(storage->line_list_nu[rpacket_get_next_line_id(packet)] - 
		   rpacket_get_nu_line(packet)) / rpacket_get_nu_line(packet) < 1e-7)
	    {
	      virtual_close_line = true;
	    }
	  // QUESTIONABLE!!!
	  bool old_close_line = rpacket_get_close_line(packet);
	  rpacket_set_close_line(packet, virtual_close_line);
	  montecarlo_one_packet(storage, packet, 1);
	  rpacket_set_close_line(packet, old_close_line);
	  virtual_close_line = false;
	}
    }
  else 
    {
      rpacket_set_tau_event(packet, rpacket_get_tau_event(packet) - tau_line);
    }
  if (!rpacket_get_last_line(packet) &&
      fabs(storage->line_list_nu[rpacket_get_next_line_id(packet)] - 
	   rpacket_get_nu_line(packet)) / rpacket_get_nu_line(packet) < 1e-7)
    {
      rpacket_set_close_line(packet, true);
    }
}

void montecarlo_bound_free_scatter(rpacket_t *packet, storage_model_t *storage,
				   double distance)
{
      rpacket_set_status(packet, TARDIS_PACKET_STATUS_REABSORBED);
}

void montecarlo_free_free_scatter(rpacket_t *packet, storage_model_t *storage,
				   double distance)
{
fprintf(stderr, "Ooups, this should not happen! Free free scattering is not implemented yet. Abort!");
exit(1);
}


inline void montecarlo_compute_distances(rpacket_t *packet, storage_model_t *storage)
{
  // Check if the last line was the same nu as the current line.
  if (rpacket_get_close_line(packet))
    {
      // If so set the distance to the line to 0.0
      rpacket_set_d_line(packet, 0.0);
      // Reset close_line.
      rpacket_set_close_line(packet, false);
    }
  else
    {
      rpacket_set_d_boundary(packet, compute_distance2boundary(packet, storage));
      double d_line;
      compute_distance2line(packet, storage, &d_line);
      rpacket_set_d_line(packet, d_line);
      //ToDo: Remove old rpacket_set_d_electron
      //rpacket_set_d_electron(packet, compute_distance2electron(packet, storage));
      compute_distance2continuum(packet, storage);
    }
}

inline montecarlo_event_handler_t montecarlo_continuum_event_handler(rpacket_t *packet, storage_model_t *storage)
{
    double zrand, normaliz_cont_th, normaliz_cont_bf, normaliz_cont_ff;
    zrand= (rk_double(&mt_state));
    normaliz_cont_th = rpacket_get_chi_electron(packet) / rpacket_get_chi_continuum(packet);
    normaliz_cont_bf = rpacket_get_chi_boundfree(packet) /  rpacket_get_chi_continuum(packet);
    normaliz_cont_ff = rpacket_get_chi_freefree(packet) /  rpacket_get_chi_continuum(packet);

    if (zrand < normaliz_cont_th)
        {
        //Return the electron scatter event function
        return &montecarlo_thomson_scatter;
        }
    else if(zrand < (normaliz_cont_th + normaliz_cont_bf))
        {
        //Return the bound-free scatter event function
        return &montecarlo_bound_free_scatter;
        }
    else
        {
        //Return the free-free scatter event function
        return &montecarlo_free_free_scatter;
        }

}



inline montecarlo_event_handler_t get_event_handler(rpacket_t *packet, storage_model_t *storage, double *distance)
{
  double d_boundary, d_continuum, d_line;
  montecarlo_compute_distances(packet, storage);
  d_boundary = rpacket_get_d_boundary(packet);
  d_continuum = rpacket_get_d_continuum(packet);
  d_line = rpacket_get_d_line(packet);
  montecarlo_event_handler_t handler;
  if (d_line <= d_boundary && d_line <= d_continuum)
    {
      *distance = d_line;
      handler =  &montecarlo_line_scatter;
    }
  else if (d_boundary <= d_continuum)
    {
      *distance = d_boundary;
      handler =  &move_packet_across_shell_boundary;
    }
  else
    {
      *distance = d_continuum;
      handler = montecarlo_continuum_event_handler(packet, storage);
    }
  return handler;
}

int64_t montecarlo_one_packet_loop(storage_model_t *storage, rpacket_t *packet, int64_t virtual_packet)
{
  rpacket_set_tau_event(packet, 0.0);
  rpacket_set_nu_line(packet, 0.0);
  rpacket_set_virtual_packet(packet, virtual_packet);
  rpacket_set_status(packet, TARDIS_PACKET_STATUS_IN_PROCESS);
  // Initializing tau_event if it's a real packet.
  if (virtual_packet == 0)
    {
      rpacket_reset_tau_event(packet);
    }
  // For a virtual packet tau_event is the sum of all the tau's that the packet passes.
  while (rpacket_get_status(packet) == TARDIS_PACKET_STATUS_IN_PROCESS)
    {
      // Check if we are at the end of line list.
      if (!rpacket_get_last_line(packet))
	{
	  rpacket_set_nu_line(packet, storage->line_list_nu[rpacket_get_next_line_id(packet)]);
	}
      double distance;
      get_event_handler(packet, storage, &distance)(packet, storage, distance);
      if (virtual_packet > 0 && rpacket_get_tau_event(packet) > 10.0)
	{
	  rpacket_set_tau_event(packet, 100.0);
	  rpacket_set_status(packet, TARDIS_PACKET_STATUS_EMITTED);
	}
    } 
  if (virtual_packet > 0)
    {
      rpacket_set_energy(packet, rpacket_get_energy(packet) * exp(-1.0 * rpacket_get_tau_event(packet)));
    }
  return rpacket_get_status(packet) == TARDIS_PACKET_STATUS_REABSORBED ? 1 : 0;
}

inline tardis_error_t rpacket_init(rpacket_t *packet, storage_model_t *storage, int packet_index, int virtual_packet_flag)
{
  double nu_line;
  double current_r;
  double current_mu;
  double current_nu;
  double comov_current_nu;
  double current_energy;
  int64_t current_line_id;
  int current_shell_id;
  bool last_line;
  bool close_line;
  int recently_crossed_boundary;
  tardis_error_t ret_val = TARDIS_ERROR_OK;
  current_nu = storage->packet_nus[packet_index];
  current_energy = storage->packet_energies[packet_index];
  current_mu = storage->packet_mus[packet_index];
  comov_current_nu = current_nu;
  current_shell_id = 0;
  current_r = storage->r_inner[0];
  current_nu = current_nu / (1 - (current_mu * current_r * storage->inverse_time_explosion * INVERSE_C));
  current_energy = current_energy / (1 - (current_mu * current_r * storage->inverse_time_explosion * INVERSE_C));
  if ((ret_val = line_search(storage->line_list_nu, comov_current_nu, storage->no_of_lines, &current_line_id)) != TARDIS_ERROR_OK)
    {
      return ret_val;
    }
  last_line = (current_line_id == storage->no_of_lines);
  recently_crossed_boundary = true;
  rpacket_set_nu(packet, current_nu);
  rpacket_set_mu(packet, current_mu);
  rpacket_set_energy(packet, current_energy);
  rpacket_set_r(packet, current_r);
  rpacket_set_current_shell_id(packet, current_shell_id);
  rpacket_set_next_line_id(packet, current_line_id);
  rpacket_set_last_line(packet, last_line);
  rpacket_set_close_line(packet, false);
  rpacket_set_recently_crossed_boundary(packet, recently_crossed_boundary);
  rpacket_set_virtual_packet_flag(packet, virtual_packet_flag);
  return ret_val;
}
