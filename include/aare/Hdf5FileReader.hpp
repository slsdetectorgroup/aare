/************************************************
 * @file HD5FFileReader.hpp
 * @short HDF5FileReader based on H5File object
 ***********************************************/

#include "Frame.hpp"
#include "NDArray.hpp"
#include <H5Cpp.h>
#include <array>
#include <cxxabi.h>
#include <optional>

namespace aare {

// return std::type_info
const std::type_info &deduce_cpp_type(const H5::DataType datatype) {
    if (H5Tequal(datatype.getId(), H5::PredType::NATIVE_UINT8.getId())) {
        return typeid(uint8_t);
    } else if (H5Tequal(datatype.getId(),
                        H5::PredType::NATIVE_UINT16.getId())) {
        return typeid(uint16_t);
    } else if (H5Tequal(datatype.getId(),
                        H5::PredType::NATIVE_UINT32.getId())) {
        return typeid(uint32_t);
    } else if (H5Tequal(datatype.getId(),
                        H5::PredType::NATIVE_UINT64.getId())) {
        return typeid(uint64_t);
    } else if (H5Tequal(datatype.getId(), H5::PredType::NATIVE_INT8.getId())) {
        return typeid(int8_t);
    } else if (H5Tequal(datatype.getId(), H5::PredType::NATIVE_INT16.getId())) {
        return typeid(int16_t);
    } else if (H5Tequal(datatype.getId(), H5::PredType::NATIVE_INT32.getId())) {
        return typeid(int32_t);
    } else if (H5Tequal(datatype.getId(), H5::PredType::NATIVE_INT64.getId())) {
        return typeid(int64_t);
    } else if (H5Tequal(datatype.getId(), H5::PredType::NATIVE_INT.getId())) {
        return typeid(int);
    } else if (H5Tequal(datatype.getId(), H5::PredType::IEEE_F64LE.getId())) {
        return typeid(double);
    } else if (H5Tequal(datatype.getId(), H5::PredType::IEEE_F32LE.getId())) {
        return typeid(float);
    } else if (H5Tequal(datatype.getId(), H5::PredType::NATIVE_FLOAT.getId())) {
        return typeid(float);
    } else if (H5Tequal(datatype.getId(),
                        H5::PredType::NATIVE_DOUBLE.getId())) {
        return typeid(float);
    } else if (H5Tequal(datatype.getId(), H5::PredType::NATIVE_CHAR.getId()) &&
               datatype.getId() == H5::PredType::NATIVE_CHAR.getId()) {
        return typeid(char);
    } else {
        throw std::runtime_error("c++ type cannot be deduced");
    }
}

struct Subset {
    std::vector<hsize_t> shape;
    std::vector<hsize_t> offset; // index where data subset should start
};

class HDF5Dataset {

  public:
    HDF5Dataset(const std::string &datasetname_, const H5::DataSet dataset_)
        : datasetname(datasetname_), dataset(dataset_) {
        datatype = dataset.getDataType();

        cpp_type = &deduce_cpp_type(datatype);

        dataspace = dataset.getSpace();
        rank = dataspace.getSimpleExtentNdims(); // number of dimensions

        shape.resize(rank);
        dataspace.getSimpleExtentDims(shape.data(), nullptr);
    }

    hsize_t get_shape(ssize_t index) { return shape[index]; }

    std::vector<hsize_t> get_shape() { return shape; }

    H5::DataType get_datatype() { return datatype; }

    const std::type_info *get_cpp_type() { return cpp_type; }

    /**
     * Reads subset of dataset into the buffer
     * e.g. to read one 2d frame pass Subset({shape[1], shape[2]}, {frame_index,
     * 0,0})
     */
    void read_into_buffer(std::byte *buffer,
                          std::optional<const Subset> subset = std::nullopt) {

        if (subset) {
            // TODO treat scalar cases
            if (static_cast<ssize_t>(subset->offset.size()) != rank) {
                throw std::runtime_error("provide an offset for" +
                                         std::to_string(rank) + "dimensions");
            }
            for (ssize_t i = 0; i < rank; ++i) {
                hsize_t size =
                    i < rank - static_cast<ssize_t>(subset->shape.size())
                        ? 0
                        : subset->shape[i - (rank - subset->shape.size())];
                if ((size + subset->offset[i]) > shape[i]) {
                    throw std::runtime_error(
                        "subset is too large or offset is out of bounds");
                }
            }

            H5::DataSpace memspace(static_cast<int>(subset->shape.size()),
                                   subset->shape.data());

            dataspace.selectHyperslab(H5S_SELECT_SET, subset->shape.data(),
                                      subset->offset.data());
            dataset.read(buffer, datatype, memspace, dataspace);
        } else {
            dataset.read(buffer, datatype);
        }
    }

    Frame store_as_frame() {
        uint32_t rows{}, cols{};
        if (rank == 1) {
            rows = 1;
            // TODO overflow
            cols = static_cast<uint32_t>(shape[0]);
        } else if (rank == 2) {
            rows = static_cast<uint32_t>(shape[0]);
            cols = static_cast<uint32_t>(shape[1]);
        } else {
            throw std::runtime_error("Frame only supports 2d images");
        }

        Frame frame(rows, cols, Dtype(*cpp_type));

        read_into_buffer(frame.data());

        return frame;
    }

    template <typename T, ssize_t NDim> NDArray<T, NDim> store_as_ndarray() {
        if (NDim != rank) {
            std::cout
                << "Warning: dataset dimension and array dimension mismatch"
                << std::endl; // TODO: replace with log - still valid if we
                              // want subset
        }
        if (typeid(T) != *cpp_type) {
            std::cout << "Warning: dataset and array type mismatch"
                      << std::endl;
        }
        std::array<ssize_t, NDim> array_shape{};
        std::transform(
            shape.begin(), shape.end(), array_shape.begin(),
            [](const auto dim) { return static_cast<ssize_t>(dim); });

        aare::NDArray<T, NDim> dataset_array(array_shape);

        read_into_buffer(reinterpret_cast<std::byte *>(dataset_array.data()));

        return dataset_array;
    }

    // getMemDataSize()

  private:
    std::string datasetname{};
    H5::DataSet dataset;
    H5::DataSpace dataspace;
    H5::DataType datatype;
    const std::type_info *cpp_type;
    ssize_t rank{};
    std::vector<hsize_t> shape{};
};

class HDF5FileReader {

  public:
    HDF5FileReader(const std::string &filename_) : filename(filename_) {
        try {
            file = H5::H5File(filename, H5F_ACC_RDONLY);
        } catch (H5::Exception &e) {
            std::cerr << "Error: " << e.getDetailMsg() << std::endl;
        }
    }

    HDF5Dataset get_dataset(const std::string &dataset_name) {
        H5::DataSet dataset;
        try {
            dataset = file.openDataSet(dataset_name);
        } catch (H5::Exception &e) {
            std::cerr << "Error: " << e.getDetailMsg() << std::endl;
        }

        // TODO use optional to handle error
        return HDF5Dataset(dataset_name, dataset);
    }

  private:
    std::string filename{};
    H5::H5File file;
};

} // namespace aare