#ifndef CHANGELOG_POLLER_H
#define CHANGELOG_POLLER_H

extern "C" {
  #include "lcap_cpp_wrapper.h"
}

std::string get_fidstr_from_path(std::string path);
int start_lcap_changelog(const std::string&, lcap_cl_ctx_ptr*, unsigned long long start_cr_index);
int poll_change_log_and_process(const std::string&, const std::string&, 
        const std::vector<std::pair<std::string, std::string> >&,
        change_map_t& change_map, lcap_cl_ctx_ptr, 
        int max_records_to_retrieve, unsigned long long& max_cr_index);
int finish_lcap_changelog(lcap_cl_ctx_ptr);

#endif

