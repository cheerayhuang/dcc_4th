#include <condition_variable>
#include <future>
#include <iostream>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>


template <int N>
class ThreadPool {

public:

    ThreadPool() {
        for (auto i = 0; i < N; ++i) {
            std::function<void()> t = [this]() {
                for (;;) {
                    TaskFunc task;
                    {
                        std::unique_lock<std::mutex> lk(mutex_);
                        cv_.wait(lk, [this]{return !tasks_.empty() || stop_;});

                        if (stop_ && tasks_.empty()) {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop_front();

                    }
                    task();
                }
            };

            threads_.emplace_back(t);
        }
    }

    ~ThreadPool() {

        {
            std::unique_lock<std::mutex> lk(mutex_);
            stop_ = true;
        }

        cv_.notify_all();

        for (auto&& th : threads_) {
            th.join();
        }
    }


    template <typename FuncType, typename... Args>
    auto enqueue(FuncType&& func, Args&&... args) {

        using RType = typename std::invoke_result<FuncType, Args...>::type;

        //std::packaged_task<RType()> p = std::bind(func, args...);

        //std::shared_ptr<std::packaged_task<RType()>> p = std::make_shared<std::packaged_task<RType()>>(std::bind(func, args...));
        auto p = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<FuncType>(func), std::forward<Args>(args)...));

        std::future<RType> future = p->get_future();


        TaskFunc task = [p] {
            (*p)();
        };

        {
            std::unique_lock<std::mutex> lk(mutex_);

            if (stop_) {
                // log a warning or error info.
            }
            //tasks_.emplace_back([p]{ (*p)(); });
            tasks_.push_back(std::move(task));
        }
        cv_.notify_one();


        return future;
    }

private:

    using TaskFunc = std::function<void()>;

    bool stop_ = false;

    std::vector<std::thread> threads_;
    std::list<TaskFunc> tasks_;

    std::condition_variable cv_;
    std::mutex mutex_;

};

int main() {
    ThreadPool<2> tp;

    auto future = tp.enqueue([](int x, int y){ return x+y; }, 2, 3);
    std::cout << future.get() << std::endl;
}
