
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <queue>
#include <memory>
#include <chrono>
#include <random>

using std::cout;
using std::endl;
using std::vector;
using std::queue;
using std::mutex;
using std::lock_guard;
using std::unique_lock;
using std::condition_variable;
using std::thread;

struct data_chunk 
{
    unsigned int m_img_id = 0;
    unsigned int m_lx = 0;
    unsigned int m_ly = 0;
    unsigned int m_h = 0;
    unsigned int m_w = 0;
    unsigned char* m_img = NULL;
    unsigned char* m_res = NULL;

};

template<typename T>
class threadsafe_queue
{
private:
    mutable std::mutex m;
    std::queue<T> m_data_queue;
    std::queue<unsigned int> m_gpuid_queue;
    std::condition_variable m_data_condi;
    int push_count = 0;
    int pop_count = 0;

public:
    threadsafe_queue()
    {
        initilGPUID();

    }

    threadsafe_queue(threadsafe_queue const& other)
    {
        std::lock_guard<std::mutex> lk(other.m);
        m_data_queue = other.m_data_queue;
    }

    void push(T new_value)
    {
        //cout << "+++push_count= " << push_count++ << endl;
        //std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 1s
        std::lock_guard<std::mutex> lk(m);
        m_data_queue.push(new_value);
        cout << "+++push_count= " << push_count++ << endl;
        //m_data_condi.notify_one();
        m_data_condi.notify_all();


    }

    void initilGPUID()
    {
        cout << "+++push_count= " << push_count++ << endl;
        std::lock_guard<std::mutex> lk(m);
        
        if (m_gpuid_queue.empty())
        {
            for (int idx_i = 0; idx_i < 4; idx_i++)
            {
                m_gpuid_queue.push(idx_i);
            }
        }

    }

    void pushGPUID(unsigned int l_gpuid)
    {        
        std::lock_guard<std::mutex> lk(m);
        m_gpuid_queue.push(l_gpuid);
        std::thread::id tid = std::this_thread::get_id();
        cout << "+++T_ID = " << tid << ", push GPU ID = " << l_gpuid << endl;
    }

    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(m);
        m_data_condi.wait(lk, [this] {return !m_data_queue.empty(); });
        value = m_data_queue.front();
        m_data_queue.pop();
        cout << "pop_count= " << pop_count++ << endl;
    }

    std::shared_ptr<T> wait_and_pop(unsigned int& r_l_gpuid)
    {
        std::unique_lock<std::mutex> lk(m);
        
        m_data_condi.wait(lk,
            [this] {
                return ((!m_data_queue.empty()) && (!m_gpuid_queue.empty()));
            });

        r_l_gpuid = m_gpuid_queue.front();
        m_gpuid_queue.pop();
        std::shared_ptr<T> res(std::make_shared<T>(m_data_queue.front()));
        m_data_queue.pop();

        std::thread::id tid = std::this_thread::get_id();
        cout << "---T_ID = " << tid << ", pop_count= " << pop_count++ << ", pop_gpuid= " << r_l_gpuid << endl;
        
        lk.unlock();
        
        return res;
    }

    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(m);
        if (m_data_queue.empty())
        {
            return false;
        }
        value = m_data_queue.front();
        m_data_queue.pop;
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(m);
        if (m_data_queue.empty())
        {
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> res(std::make_shared<T>(m_data_queue.front()));
        m_data_queue.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(m);
        return m_data_queue.empty();
    }

};

data_chunk prepare_data()
{
    return data_chunk();
}

//static unsigned int total_count_processing = 0;
//static unsigned int total_elapsed_time_processing_16 = 0;

void process_data(data_chunk& r_l_data, unsigned int l_gpuid) 
{

    std::random_device rd;
    std::default_random_engine generator(rd());
    std::uniform_real_distribution<double> unif(0.1, 0.9);

    /* ²£¥Í¶Ã¼Æ */
    double random_num_base = unif(generator);
    unsigned int random_num_ms = (unsigned int)(random_num_base * 100.0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(random_num_ms));
    std::thread::id tid = std::this_thread::get_id();
    cout << "$$$$$$$$$$$$$$$$$T_ID = " << tid << endl;
    /*total_count_processing++;
    total_elapsed_time_processing_16 += random_num_ms;
    if (0 == (total_count_processing % 16))
    {
        cout << "==============elapsed time processing 16 data: "<< total_elapsed_time_processing_16 << "\n";
        total_elapsed_time_processing_16 = 0;
    }*/
    //cout << "===processing data end\n";
}

void data_processing_thread(threadsafe_queue<data_chunk>& r_l_data_queue)
{
    while (true)
    {

        //std::shared_ptr<data_chunk> dataPtr = r_l_data_queue.wait_and_pop();
        
        unsigned int gpuid = 0;
        std::shared_ptr<data_chunk> dataPtr = r_l_data_queue.wait_and_pop(gpuid);
        if (dataPtr) {
            process_data(*dataPtr, gpuid);
        }
        

        r_l_data_queue.pushGPUID(gpuid);
    }
}

void data_preparation_thread(threadsafe_queue<data_chunk>& r_l_data_queue)
{

    for (int i = 0; i < 1000; i++)
    {
        data_chunk const data = prepare_data();
        r_l_data_queue.push(data);

    }

}

int main()
{
    threadsafe_queue<data_chunk> data_queue;

    thread t1_1(data_preparation_thread, std::ref(data_queue));
    t1_1.join();

    thread t0(data_processing_thread, std::ref(data_queue));
    thread t1(data_processing_thread, std::ref(data_queue));
    thread t2(data_processing_thread, std::ref(data_queue));
    thread t3(data_processing_thread, std::ref(data_queue));
    thread t4(data_processing_thread, std::ref(data_queue));
    thread t5(data_processing_thread, std::ref(data_queue));
    thread t6(data_processing_thread, std::ref(data_queue));
    thread t7(data_processing_thread, std::ref(data_queue));
    thread t8(data_processing_thread, std::ref(data_queue));
    thread t9(data_processing_thread, std::ref(data_queue));
    thread t10(data_processing_thread, std::ref(data_queue));
    thread t11(data_processing_thread, std::ref(data_queue));
    thread t12(data_processing_thread, std::ref(data_queue));
    thread t13(data_processing_thread, std::ref(data_queue));
    thread t14(data_processing_thread, std::ref(data_queue));
    thread t15(data_processing_thread, std::ref(data_queue));
    
    t0.join();
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();
    t7.join();
    t8.join();
    t9.join();
    t10.join();
    t11.join();
    t12.join();
    t13.join();
    t14.join();
    t15.join();

}
