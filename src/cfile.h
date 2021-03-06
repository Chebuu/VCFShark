#pragma once
// *******************************************************************************************
// This file is a part of VCFShark software distributed under GNU GPL 3 licence.
// The homepage of the VCFShark project is https://github.com/refresh-bio/VCFShark
//
// Author : Sebastian Deorowicz and Agnieszka Danek
// Version: 1.0
// Date   : 2020-12-18
// *******************************************************************************************

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <utility>

#include "defs.h"
#include "bsc.h"
#include "io.h"
#include "pbwt.h"
#include "rc.h"
#include "sub_rc.h"
#include "vcf.h"
#include "archive.h"
#include <unordered_map>
#include <unordered_set>
#include "context_hm.h"
#include "buffer.h"
#include "queue.h"
#include "text_pp.h"
#include "format.h"
#include "graph_opt.h"

using namespace std;

// ************************************************************************************
class CCompressedFile
{
	CVectorIOStream *vios_i;
	CVectorIOStream *vios_o;

	vector<uint8_t> v_vios_i;
	vector<uint8_t> v_vios_o;

	vector<CBuffer> v_o_buf;
	vector<CBuffer> v_i_buf;
	vector<int> v_buf_ids_size;
	vector<int> v_buf_ids_data;
	vector<int> v_buf_ids_func;

	vector<CBuffer> v_o_db_buf;
	vector<CBuffer> v_i_db_buf;
	vector<int> v_db_ids_size;
	vector<int> v_db_ids_data;

	const uint32_t id_db_chrom = 0;
	const uint32_t id_db_pos = 1;
	const uint32_t id_db_id = 2;
	const uint32_t id_db_ref = 3;
	const uint32_t id_db_alt = 4;
	const uint32_t id_db_qual = 5;
	const uint32_t no_db_fields = 6;

	const array<string, 6> db_stream_name_size = { "db_chrom_size", "db_pos_size", "db_id_size", "db_ref_size", "db_alt_size", "db_qual_size" };
	const array<string, 6> db_stream_name_data = { "idb_chrom_data", "idb_pos_data", "idb_id_data", "idb_ref_data", "idb_alt_data", "idb_qual_data" };

	vector<CBSCWrapper*> v_bsc_size;
	vector<CBSCWrapper*> v_bsc_data;
	vector<CTextPreprocessing> v_text_pp;

	vector<CBSCWrapper*> v_bsc_db_size;
	vector<CBSCWrapper*> v_bsc_db_data;

	vector<CFormatCompress*> v_format_compress;

	vector<thread> v_coder_threads;
	mutex mtx_v_coder;
	mutex mtx_v_text;
	condition_variable cv_v_coder;
	condition_variable cv_v_text;
	vector<uint32_t> v_coder_part_ids;
	vector<uint32_t> v_text_part_ids;

#ifdef LOG_INFO
	unordered_map<int, unordered_set<int>> distinct_values;
#endif

	struct SPackage {
		enum class package_t {fields, gt, db};

		package_t type;
		int key_id;
		int db_id;
		uint32_t stream_id_size;
		uint32_t stream_id_data;
		int part_id;
		vector<uint32_t> v_size;
		vector<uint8_t> v_data;
		vector<uint8_t> v_compressed;

		function_data_item_t fun;
		int stream_id_src;
		bool is_func;

		SPackage()
		{
			type = package_t::fields;
			key_id = -1;
			db_id = -1;
			stream_id_size = 0;
			stream_id_data = 0;
			part_id = -1;
			stream_id_src = -1;
			is_func = false;
		}

		SPackage(SPackage::package_t _type, int _key_id, int _db_id, uint32_t _stream_id_size, uint32_t _stream_id_data, int _part_id, vector<uint32_t>& _v_size, vector<uint8_t>& _v_data, vector<uint8_t>& _v_compressed)
		{
			type = _type;
			key_id = _key_id;
			db_id = _db_id;
			stream_id_size = _stream_id_size;
			stream_id_data = _stream_id_data;
			stream_id_src = -1;
			part_id = _part_id;
			v_size = move(_v_size);
			v_data = move(_v_data);
			v_compressed = move(_v_compressed);
			is_func = false;

			_v_size.clear();
			_v_data.clear();
			_v_compressed.clear();
		}

		SPackage(SPackage::package_t _type, int _key_id, int _db_id, uint32_t _stream_id_size, uint32_t _stream_id_data, int _part_id, vector<uint32_t>& _v_size, int _stream_id_src, function_data_item_t& _fun)
		{
			type = _type;
			key_id = _key_id;
			db_id = _db_id;
			stream_id_size = _stream_id_size;
			stream_id_data = _stream_id_data;
			stream_id_src = _stream_id_src;
			part_id = _part_id;
			is_func = true;

			fun = move(_fun);
		}
	};

	CArchive *archive;
	CArchive *tmp_archive;
	string archive_name;

	CRegisteringQueue<SPackage>* q_packages;
	CRegisteringQueue<pair<int, int>>* q_preparation_ids;

	vector<SPackage*> v_packages;
	vector<SPackage*> v_db_packages;
	vector<int> v_cnt_packages;
	vector<int> v_cnt_db_packages;
	mutex m_packages;
	condition_variable cv_packages;

	//const uint32_t max_buffer_size = 16 << 20;
	const uint32_t max_buffer_size = 8 << 20;
	const uint32_t max_buffer_gt_size = 256 << 20;
	const uint32_t max_buffer_db_size = 8 << 20;

	const size_t pp_compress_flag = 1u << 30;
	const int max_cnt_packages = 3;

	const bsc_params_t p_bsc_size = { 25, 16, 128, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_data = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_flag = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_text = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_int = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_real = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };

	const bsc_params_t p_bsc_db_chrom = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_db_pos = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_db_id = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_db_ref = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_db_alt = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	const bsc_params_t p_bsc_db_qual = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };
	
	const bsc_params_t p_bsc_meta = { 25, 16, 64, LIBBSC_CODER_QLFC_ADAPTIVE };

	const uint32_t p_bsc_features = 1u;
//	const uint32_t p_bsc_features = 0u;

	CRangeEncoder<CVectorIOStream> *rce;
	CRangeDecoder<CVectorIOStream> *rcd;

    CPBWT pbwt;
	bool pbwt_initialised;
	uint32_t no_coder_threads;

	enum class open_mode_t {none, reading, writing} open_mode;

	vector<uint8_t> v_rd_header, v_cd_header;
	vector<uint8_t> v_rd_meta, v_cd_meta;
	vector<uint8_t> v_rd_samples, v_cd_samples;

	vector<uint8_t> v_rd_chrom, v_cd_chrom;
	vector<uint8_t> v_rd_pos, v_cd_pos;
	vector<uint8_t> v_rd_id, v_cd_id;
	vector<uint8_t> v_rd_ref, v_cd_ref;
	vector<uint8_t> v_rd_alt, v_cd_alt;
	vector<uint8_t> v_rd_qual, v_cd_qual;

	size_t p_meta;
	size_t p_header;
	size_t p_samples;

	uint32_t no_variants;
	uint32_t i_variant;
	uint32_t no_samples;
    uint32_t no_keys;
	uint8_t ploidy;
	uint32_t neglect_limit;
	string v_meta;
	string v_header;
	vector<string> v_samples;

    vector<key_desc> keys;
    int gt_key_id;
	int gt_stream_id;
    
	int64_t prev_pos;

	const context_t context_symbol_flag = 1ull << 60;
	const context_t context_symbol_mask = 0xffff;

	const context_t context_prefix_mask = 0xfffff;
	const context_t context_prefix_flag = 2ull << 60;
	const context_t context_suffix_flag = 3ull << 60;
	const context_t context_large_value1_flag = 4ull << 60;
	const context_t context_large_value2_flag = 5ull << 60;
	const context_t context_large_value3_flag = 6ull << 60;
	
	context_t ctx_prefix;
	context_t ctx_symbol;
	
	typedef CContextHM<CRangeCoderModel<CSimpleModel, CVectorIOStream>> ctx_map_e_t;
	typedef CContextHM<CRangeCoderModel<CSimpleModel, CVectorIOStream>> ctx_map_d_t;

	ctx_map_e_t rce_coders;
	ctx_map_d_t rcd_coders;

	function_data_graph_t function_data_graph;
	function_size_graph_t function_size_graph;

	vector<pair<int, bool>> v_size_nodes;
	vector<pair<int, int>> v_size_edges;
	vector<pair<int, bool>> v_data_nodes;
	vector<pair<int, int>> v_data_edges;
	vector<bool> m_data_nodes;
	vector<int> m_data_edges;

	inline ctx_map_e_t::value_type find_rce_coder(context_t ctx, uint32_t no_symbols, uint32_t max_log_counter);
	inline ctx_map_d_t::value_type find_rcd_coder(context_t ctx, uint32_t no_symbols, uint32_t max_log_counter);

	inline void encode_run_len(uint32_t symbol, uint32_t len);
	inline void decode_run_len(uint32_t &symbol, uint32_t &len);

	void append(vector<uint8_t> &v_comp, string x);
	void append(vector<uint8_t> &v_comp, int64_t x);
	void append_fixed(vector<uint8_t> &v_comp, uint64_t x, int n);

	void read(vector<uint8_t> &v_comp, size_t &pos, string &x);
	void read(vector<uint8_t> &v_comp, size_t &pos, int64_t &x);
	void read(vector<uint8_t> &v_comp, size_t &pos, uint64_t &x);
	void read(vector<uint8_t> &v_comp, size_t &pos, uint32_t &x);
	void read_fixed(vector<uint8_t> &v_comp, size_t &pos, uint64_t &x, int n);

	bool load_descriptions();
	bool save_descriptions();

	void lock_coder_compressor(SPackage& pck);
	void unlock_coder_compressor(SPackage& pck);
	void lock_text_compressor(SPackage& pck);
	void unlock_text_compressor(SPackage& pck);
	void skip_text_compressor(SPackage& pck);

	void compress_field(SPackage& pck, vector<uint8_t> &v_compressed, vector<uint8_t> &v_tmp);
	void decompress_field(SPackage* pck, size_t raw_size, vector<uint8_t>& v_tmp);

	void compress_format(SPackage& pck, vector<uint8_t> &v_compressed, vector<uint8_t> &v_tmp);
	void compress_info(SPackage& pck, vector<uint8_t> &v_compressed, vector<uint8_t> &v_tmp);
	void decompress_format(SPackage* pck, size_t raw_size, vector<uint8_t>& v_tmp);
	void decompress_info(SPackage* pck, size_t raw_size, vector<uint8_t>& v_tmp);

	void compress_gt(SPackage& pck);
	void decompress_gt(SPackage* pck, size_t raw_size);

	void compress_db(SPackage& pck, vector<uint8_t>& v_compressed, vector<uint8_t>& v_tmp);
	void decompress_db(SPackage* pck, size_t raw_size, vector<uint8_t>& v_tmp);

	bool process_function_size(int no_keys, vector<pair<int, bool>>& v_out_nodes, vector<pair<int, int>>& v_out_edges);
	bool process_function_data(int no_keys, vector<pair<int, bool>>& v_out_nodes, vector<pair<int, int>>& v_out_edges);
	bool process_function_data_eq_only(int no_keys, vector<pair<int, bool>>& v_out_nodes, vector<pair<int, int>>& v_out_edges);

	void store_nodes(string stream_name, vector<pair<int, bool>>& v_nodes);
	void store_edges(string stream_name, vector<pair<int, int>>& v_edges, int no_keys);
	void load_nodes(string stream_name, vector<pair<int, bool>>& v_nodes);
	void load_edges(string stream_name, vector<pair<int, int>>& v_edges, int no_keys);

	void copy_stream(string stream_name);
	void link_stream(string stream_name, string target_name);
	void store_function(string stream_name, int src_id, function_size_item_t& func);
	void store_function(string stream_name, int src_id, function_data_item_t& func);
	void load_function(string stream_name, int &src_id, function_size_item_t& func);
	void load_function(string stream_name, int &src_id, function_data_item_t& func);

public:
	CCompressedFile();
	~CCompressedFile();

	bool OpenForReading(string file_name);
	bool OpenForWriting(string file_name, uint32_t _no_keys);
	bool OptimizeDB(function_size_graph_t&_function_size_graph, function_data_graph_t& _function_data_graph);
	bool Close();

    int GetNoSamples();
    void SetNoSamples(uint32_t _no_samples);
	int GetNoVariants();
	bool GetMeta(string &_v_meta);
	bool SetMeta(string &_v_meta);
	bool GetHeader(string &_v_header);
	bool SetHeader(string &_v_header);

    bool AddSamples(vector<string> &_v_samples);    
    bool GetSamples(vector<string> &_v_samples);
    
    bool SetKeys(vector<key_desc> &_keys);
    bool GetKeys(vector<key_desc> &_keys);
    
    int GetNoKeys();
    void SetNoKeys(uint32_t _no_keys);
    
    int GetGTId();
    void SetGTId(uint32_t _gt_key_id);
    
	int GetPloidy();
	void SetPloidy(int _ploidy);

	void SetNoThreads(int _no_threads);

	int GetNeglectLimit();
	void SetNeglectLimit(uint32_t _neglect_limit);

	bool Eof();

	bool GetVariant(variant_desc_t &desc, vector<field_desc> &fields);
	bool SetVariant(variant_desc_t &desc, vector<field_desc> &fields);
    
    bool InitPBWT();
};

// EOF
