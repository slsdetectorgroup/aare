#pragma once
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <fmath>
#include <cstddef>

//JMulvey
//This is a new way to do pedestals (inspired by Dominic's cluster finder)
//Instead of pedestal tracking, we split the data (photon data) up into chunks (say 50K frames)
//For each chunk, we look at the spectra and fit to the noise peak. When we run the cluster finder, we then use this chunked pedestal data
//The smaller the chunk size, the more accurate, but also the longer it takes to process.
//It is essentially a pre-processing step.
//Ideally this new class will do that processing. 
//But for now we will just implement a method to pass in the chunked pedestal values directly (I have my own script which does it for now)
//I've cut this down a lot, knowing full well it'll need changing if we want to merge it with main (happy to do that once I get it work for what I need)

namespace aare {

/**
 * @brief Calculate the pedestal of a series of frames. Can be used as
 * standalone but mostly used in the ClusterFinder.
 *
 * @tparam SUM_TYPE type of the sum
 */
template <typename SUM_TYPE = double> class ChunkedPedestal {
    uint32_t m_rows;
    uint32_t m_cols;
    uint32_t m_n_chunks
    uint64_t m_current_frame_number
    uint64_t m_current_chunk_number

    NDArray<SUM_TYPE, 3> m_mean;
    NDArray<SUM_TYPE, 3> m_std;
    uint32_t m_chunk_size

  public:
    Pedestal(uint32_t rows, uint32_t cols, uint32_t chunk_size = 50000, uint32_t n_chunks = 10)
        : m_rows(rows), m_cols(cols), m_chunk_size(chunk_size), m_n_chunks(n_chunks)    
          m_mean(NDArray<SUM_TYPE, 2>({rows, cols, n_chunks})) {
        assert(rows > 0 && cols > 0 && chunk_size > 0);
        m_mean = 0;
        m_std = 0;
    }
    ~Pedestal() = default;

    NDArray<SUM_TYPE, 3> mean() { return m_mean; }
    NDArray<SUM_TYPE, 3> std() { return m_std; }

    void set_frame_number (uint64_t frame_number) {
        m_current_frame_number = frame_number
        uint32_t chunk_number = floor(frame_number / m_chunk_size)

        if (chunk_number >= m_n_chunks) 
        {
            chunk_number = 0;
            throw std::runtime_error(
                "Chunk number exceeds the number of chunks");
        }
    }

    SUM_TYPE mean(const uint32_t row, const uint32_t col) const {    
        return m_mean(row, col, m_current_chunk_number);
    }

    SUM_TYPE std(const uint32_t row, const uint32_t col) const {

        uint32_t chunk_number = floor(frame_number / m_chunk_size)
        if (chunk_number >= m_n_chunks) return 0;

        return m_std(row, col, chunk_number);
    }

    void clear() {
        m_mean = 0;
        m_std = 0;
        m_n_chunks = 0;
    }

    //Probably don't need to do this one at a time, but let's keep it simple for now
    template <typename T> void push_mean(NDView<T, 2> frame, uint32_t chunk_number) {
        assert(frame.size() == m_rows * m_cols);

        // TODO! move away from m_rows, m_cols
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                push_mean<T>(row, col, chunk_number, frame(row, col));
            }
        }
    }
    
    template <typename T> void push_std(NDView<T, 2> frame, uint32_t chunk_number) {
        assert(frame.size() == m_rows * m_cols);

        // TODO! move away from m_rows, m_cols
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                push_std<T>(row, col, chunk_number, frame(row, col));
            }
        }
    }

    // pixel level operations (should be refactored to allow users to implement
    // their own pixel level operations)
    template <typename T>
    void push_mean(const uint32_t row, const uint32_t col, const uint32_t chunk_number, const T val_) {        
        m_mean(row, col, chunk_number) = val_
    }

    template <typename T>
    void push_std(const uint32_t row, const uint32_t col, const uint32_t chunk_number, const T val_) {        
        m_std(row, col, chunk_number) = val_
    }

    // getter functions
    uint32_t rows() const { return m_rows; }
    uint32_t cols() const { return m_cols; }

};

} // namespace aare