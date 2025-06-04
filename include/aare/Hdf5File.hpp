#pragma once
#include "aare/FileInterface.hpp"
#include "aare/Frame.hpp"
#include "aare/Hdf5MasterFile.hpp"
#include "aare/NDArray.hpp" //for pixel map

#include <optional>

namespace aare {

struct H5Handles {
  std::string file_name{};
  std::string dataset_name{};
  H5::H5File file;
  H5::DataSet dataset;
  H5::DataSpace dataspace;
  H5::DataType datatype;
  std::unique_ptr<H5::DataSpace> memspace{nullptr};
  std::vector<hsize_t>dims;
  std::vector<hsize_t> count;
  std::vector<hsize_t> offset;

  H5Handles(const std::string& fname, const std::string& dname):
    file_name(fname),
    dataset_name(dname),
    file(fname, H5F_ACC_RDONLY),
    dataset(file.openDataSet(dname)),
    dataspace(dataset.getSpace()),
    datatype(dataset.getDataType())
  {
    // get dimensions
    int rank = dataspace.getSimpleExtentNdims();
    dims.resize(rank);
    dataspace.getSimpleExtentDims(dims.data(), nullptr);

    // to slice 1 frame with hyperslab    
    count.push_back(1);
    offset.push_back(0);

    // header datasets
    if (rank == 1) {
      memspace = std::make_unique<H5::DataSpace>(H5S_SCALAR);
    }  
    // header virtual datasets
    else if (rank == 2) {
      memspace = std::make_unique<H5::DataSpace>(H5S_SCALAR);
      count.push_back(1);
      offset.push_back(0);
    } 
    // data dataset
    else {
      hsize_t dimsm[2] = {dims[1], dims[2]};
      memspace = std::make_unique<H5::DataSpace>(2, dimsm);
      count.push_back(dims[1]);
      count.push_back(dims[2]);
      offset.push_back(0);
      offset.push_back(0);
    }
  };

  void seek(size_t frame_index) {
    if (frame_index >= dims[0]) {
        throw std::runtime_error(LOCATION + "Invalid frame number");
    }
    offset[0] = static_cast<hsize_t>(frame_index);
  };

  void get_frame_into(size_t frame_index, std::byte *frame_buffer) {
    seek(frame_index);
    //LOG(logDEBUG) << "data offset:" << offset << " count:" << count;
    dataspace.selectHyperslab(H5S_SELECT_SET, count.data(), offset.data());
    dataset.read(frame_buffer, datatype, *memspace, dataspace);
  };

  void get_header_into(size_t frame_index, int part_index, std::byte *header_buffer) {
    seek(frame_index);
    offset[1] = part_index;
    //LOG(logDEBUG) << "header offset:" << offset << " count:" << count;
    dataspace.selectHyperslab(H5S_SELECT_SET, count.data(), offset.data());
    dataset.read(header_buffer, datatype, *memspace, dataspace);
  };
};

/**
 * @brief Class to read .h5 files. The class will parse the master file
 * to find the correct geometry for the frames.
 * @note A more generic interface is available in the aare::File class.
 * Consider using that unless you need hdf5 file specific functionality.
 */
class Hdf5File : public FileInterface {
    Hdf5MasterFile m_master;

    size_t m_current_frame{};
    size_t m_total_frames{};
    size_t m_rows{};
    size_t m_cols{};

  public:
    /**
     * @brief Hdf5File constructor
     * @param fname path to the master file (.json)
     * @param mode file mode (only "r" is supported at the moment)

     */
    Hdf5File(const std::filesystem::path &fname, const std::string &mode = "r");
    virtual ~Hdf5File() override;

    Frame read_frame() override;
    Frame read_frame(size_t frame_number) override;
    std::vector<Frame> read_n(size_t n_frames) override;
    void read_into(std::byte *image_buf) override;
    void read_into(std::byte *image_buf, size_t n_frames) override;

    // TODO! do we need to adapt the API?
    void read_into(std::byte *image_buf, DetectorHeader *header);
    void read_into(std::byte *image_buf, size_t n_frames,
                   DetectorHeader *header);

    size_t frame_number(size_t frame_index) override;
    size_t bytes_per_frame() override;
    size_t pixels_per_frame() override;
    size_t bytes_per_pixel() const;
    void seek(size_t frame_index) override;
    size_t tell() override;
    size_t total_frames() const override;
    size_t rows() const override;
    size_t cols() const override;
    size_t bitdepth() const override;
    xy geometry();
    size_t n_modules() const;
    Hdf5MasterFile master() const;

    DetectorType detector_type() const override;

  private:
    /**
     * @brief read the frame at the given frame index into the image buffer
     * @param frame_number frame number to read
     * @param image_buf buffer to store the frame
     */
    void get_frame_into(size_t frame_index, std::byte *frame_buffer,
                        DetectorHeader *header = nullptr);
    
    /**
     * @brief read the frame at the given frame index into the image buffer
     * @param frame_index frame number to read
     * @param frame_buffer buffer to store the frame
     */
    void get_data_into(size_t frame_index, std::byte *frame_buffer);
    
    /**
     * @brief read the header at the given frame index into the header buffer
     * @param frame_index frame number to read
     * @param part_index part index to read (for virtual datasets)
     * @param header buffer to store the header
     */
    void get_header_into(size_t frame_index, int part_index, DetectorHeader *header);
    
    /**
     * @brief get the frame at the given frame index
     * @param frame_number frame number to read
     * @return Frame
     */
    Frame get_frame(size_t frame_index);

    /**
     * @brief read the header of the file
     * @param fname path to the data subfile
     * @return DetectorHeader
     */
    static DetectorHeader read_header(const std::filesystem::path &fname);

    static const std::string metadata_group_name;
    static const std::vector<std::string> header_dataset_names;

    std::unique_ptr<H5Handles> m_data_file{nullptr};
    std::vector<std::unique_ptr<H5Handles>> m_header_files;

    void open_data_file();
    void open_header_files();
};

} // namespace aare