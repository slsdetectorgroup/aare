// SPDX-License-Identifier: MPL-2.0
// File readers: everything that opens a detector file.
#include "module_config.hpp"

#include "bind_DataSetAggregation.hpp"
#include "bind_MultiThreadedFileReader.hpp"
#include "bind_RawFile.hpp"
#include "ctb_raw_file.hpp"
#include "file.hpp"
#include "jungfrau_data_file.hpp"
#include "raw_master_file.hpp"
#include "raw_sub_file.hpp"

void bind_dataset_aggregation(py::module &m) {
    bind_DataSetAggregation<uint16_t>(m);
    bind_read_current_source(m);
}
