#pragma once
// *******************************************************************************************
// This file is a part of VCFShark software distributed under GNU GPL 3 licence.
// The homepage of the VCFShark project is https://github.com/refresh-bio/VCFShark
//
// Author : Sebastian Deorowicz and Agnieszka Danek
// Version: 1.0
// Date   : 2020-12-18
// *******************************************************************************************

#include <vector>
#include <string>

using namespace std;

enum class work_mode_t {none, compress, decompress};
enum class file_type {VCF, BCF};

// ************************************************************************************
struct CParams
{
	work_mode_t work_mode;

	string vcf_file_name;
	string db_file_name;
	string sample_file_name;
	string id_sample;
	bool store_sample_header;
    
    file_type out_type;
    char bcf_compression_level;
	bool extra_variants;

	// internal params
	uint32_t neglect_limit;
	uint32_t no_threads;

	CParams()
	{
		work_mode = work_mode_t::none;
		no_threads = 1;
		store_sample_header = false;
        
        out_type = file_type::VCF;
        bcf_compression_level = '1';
		extra_variants = false;
		no_threads = 8;

		// internal params
		neglect_limit = 10;
	}

	void store_params(vector<uint8_t> &v_params)
	{
		v_params.push_back('G');
		v_params.push_back('T');
		v_params.push_back('S');
		v_params.push_back('1');

		v_params.push_back((uint8_t)neglect_limit);
	}

	bool load_params(vector<uint8_t> &v_params)
	{
		int i = 0;

		if (v_params.size() != 5)
			return false;

		if (v_params[i++] != 'G')	return false;
		if (v_params[i++] != 'T')	return false;
		if (v_params[i++] != 'S')	return false;
		if (v_params[i++] != '1')	return false;

		neglect_limit = v_params[i++];

		return true;
	}
};

// EOF
