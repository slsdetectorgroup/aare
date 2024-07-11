
#include "aare/file_io.hpp"
#include "aare/processing/Pedestal.hpp"
#include "aare/processing/ClusterFinder.hpp"

#include <fmt/format.h>
#include <chrono>
using namespace std::chrono;
#include <thread>
#include <memory>
#include "aare/core/ProducerConsumerQueue.hpp"


using Queue = folly::ProducerConsumerQueue<aare::Frame>;

//Small wrapper to do cluster finding in a thread
class ThreadedClusterFinder{
    std::atomic<bool> m_stop = false;
    std::atomic<size_t> m_frames_processed = 0;
    Queue* q =nullptr;
    aare::Pedestal<double> pd;
    int id;
public:

    
    ThreadedClusterFinder(Queue& q, aare::Pedestal<double> pd, int id):q(&q),pd(pd),id(id){
  
    }


    size_t frames_processed() const{
        return m_frames_processed;
    }
    void request_stop(){
        fmt::print("Stop requested\n");
        m_stop = true;
    }

    void find_clusters(){
        // aare::ClusterFinder cf(3, 3, 5, 0);
        while(!m_stop){
            aare::ClusterFinder cf(3, 3, 5, 0);
            aare::Frame frame(1,1,aare::Dtype("u4"));
            if (q->read(frame)){
                auto clusters = cf.find_clusters_without_threshold(frame.view<uint16_t>(), pd, true);
                m_frames_processed++;
                // fmt::print("{}:Found {} clusters\n", id,clusters.size());
            }else{
                fmt::print("{}:Queue empty\n", id);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    
    }


};

int main(){
    std::filesystem::path fname = "/Users/erik/data/cluster/cu_half_speed_master_4.json";
    fmt::print("Loading {}\n", fname.c_str());

    

    aare::Pedestal<double> pd(400,400,1000);
    aare::File f(fname, "r");

    //Use the first 1000 frames to calculate the pedestal
    auto t0 = high_resolution_clock::now();
    for (int i = 0; i<1000; ++i){
        aare::Frame frame = f.iread(i);
        pd.push<uint16_t>(frame);
    }
    auto t1 = high_resolution_clock::now();
    fmt::print("Pedestal run took: {}s\n", duration_cast<microseconds>(t1 - t0).count()/1e6);
    

    //Make N queues to put frames in
    std::vector<Queue> queues;
    const int n_threads = 10;
    const uint32_t queue_size = 2000;
    for (int i = 0; i<n_threads; ++i){
        queues.emplace_back(queue_size);
    }

    std::vector<std::unique_ptr<ThreadedClusterFinder>> cluster_finders;
    for (int i = 0; i<n_threads; ++i){
        cluster_finders.push_back(std::make_unique<ThreadedClusterFinder>(queues[i], pd, i));
    }

    std::vector<std::thread> threads;
    for (int i = 0; i<n_threads; ++i){
        threads.emplace_back(&ThreadedClusterFinder::find_clusters, cluster_finders[i].get());
    }

    //Push frames to the queues
    const int n_frames = 1000;
    for (int i = 0; i<n_frames; ++i){
        //if the Queue is full, wait, there are better ways to do this =) 
        while (queues[i%n_threads].isFull()){
            fmt::print("Queue {} is full, waiting\n", i%n_threads);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        queues[i%n_threads].write(f.iread(i));
        if(i%100==0){
            fmt::print("Pushed frame {}\n", i);
        }
    }

    // std::this_thread::sleep_for(std::chrono::seconds(3));


    //wait for all queues to be empty
    for (auto& q: queues){
        while(!q.isEmpty()){
            fmt::print("Finish Queue not empty, waiting\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }



    
    for (auto& cf: cluster_finders){
        cf->request_stop();
    }
    for(auto& t: threads){
        t.join();
    }

    size_t total_frames = 0;
    for (auto& cf: cluster_finders){
        total_frames += cf->frames_processed();
    }
    auto t2 = high_resolution_clock::now();
    fmt::print("Processed {} frames in {}s\n", total_frames,duration_cast<microseconds>(t2 - t1).count()/1e6);




    // auto start = high_resolution_clock::now();
    // aare::ClusterFinder cf(3, 3, 5, 0);
    // for (int i = 1000; i<2000; ++i){
    //     aare::Frame frame = f.iread(i);
    //     auto clusters = cf.find_clusters_without_threshold(frame.view<uint16_t>(), pd, true);
    // }

    // auto stop = high_resolution_clock::now();
    // auto duration = duration_cast<microseconds>(stop - start);
    // fmt::print("Run took: {}s\n", duration.count()/1e6);


}