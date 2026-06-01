#include "AllVBManager.h"

#include <tbb/blocked_range.h>
#include <tbb/parallel_for_each.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>

#define MIN_VERSION_UPDATER_INTERVAL 100

void XorShiftRandom::seed(uint64_t s) {
  state = s ? s : 1;  // state不能为0
}

uint32_t XorShiftRandom::rand_int(uint32_t max) {
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state % max;
}
thread_local uint64_t XorShiftRandom::state =
    std::chrono::high_resolution_clock::now().time_since_epoch().count();

thread_local size_t AllVBManager::thread_id = 0;
void AllVBManager::register_thread(size_t id, bool on) {
  lock_guard<mutex> l(thread_registry_lock);
  // std::cout << "Registering thread " << id << "\n";
  if (thread_id_in_use[id] && on) {
    std::cout << "Tyring to reuse a thread id.\n" << on << '\n';
    assert(false);
  }
  thread_id_in_use[id] = true;
  thread_id = id;
}

epoch_t AllVBManager::getMinActiveVersion() { return min_epoch; }

void AllVBManager::update_min_version() {
  min_read_epoch = read_epoch_;
  epoch_t min1 =
      std::accumulate(active_read_epochs.begin(), active_read_epochs.end(),
                      numeric_limits<epoch_t>::max(),
                      [](epoch_t a, epoch_t b) { return std::min(a, b); });

  min_read_epoch = std::min(min_read_epoch, min1);

  auto last = min_epoch;
  min_epoch = (cur_ + no_more_txn_);
  auto min2 =
      std::accumulate(active_transactions.begin(), active_transactions.end(),
                      numeric_limits<epoch_t>::max(),
                      [](epoch_t a, epoch_t b) { return std::min(a, b); });

  min_epoch = std::min(min_epoch, min2);
  if (min_epoch < last && min2 != numeric_limits<epoch_t>::max()) {
    // std::cout << "yes\n";
    std::cout << "yes " << last << " " << min_epoch << '\n';
    min_epoch = last;
  }
}

void AllVBManager::GC() {
#ifdef FINEGRAIN
  while (!vb_que_.empty() &&
         vb_que_.front()->get_epoch() < min_read_epoch) {  // fine-grained
#else
  while (!vb_que_.empty() && vb_que_.front()->get_epoch() <= min_read_epoch) {
#endif
    // std::cout << "Garbage collection " << vb_que_.front()->get_epoch() <<
    // '\n';
    delete vb_que_.front();
    vb_que_.pop();
  }
}
/**
 * Periodically updates the minimal epoch of all active transactions.
 *
 * @param interval The interval in which the minimal epoch is updated, in
 * microseconds.
 */
void AllVBManager::run_min_epoch_updater(uint interval) {
  unsigned last_min_epoch = 0;
  double st = 0;
  while ((!no_more_txn_ || read_epoch_ != cur_) || !stopped) {
    update_min_version();
    GC();
    if (read_epoch_ + 1 < min_epoch) {
      // std::cout << "sleep time : " << to_string(st / 10)
      //           << "ms  read_epoch_ = " << to_string(read_epoch_)
      //           << " min_epoch = " + to_string(min_epoch) + '\n';
      // st = 0;
      commitVersionBlock(min_epoch);
    }
    this_thread::sleep_for(chrono::microseconds(interval));
    // if (start_count) st++;
    // auto start = std::chrono::steady_clock::now();
    // auto timeout = std::chrono::microseconds(interval);

    // while (std::chrono::steady_clock::now() - start <
    //        chrono::microseconds(interval));
  }
}

void AllVBManager::deregister_thread(size_t id) {
  lock_guard<mutex> l(thread_registry_lock);
  if (!thread_id_in_use[id]) {
    std::cout << "Trying to deregister a thread that has not been registered\n";
    assert(false);
  }
  if (active_transactions[id] != MY_NO_TRANSACTION) {
    std::cout << "Trying to deregister a thread with an active transaction\n";
    assert(false);
  }
  thread_id_in_use[id] = false;
}

void AllVBManager::reset_max_threads(uint max_threads) {
  lock_guard<mutex> l(thread_registry_lock);
  for (bool in_use : thread_id_in_use) {
    if (in_use) {
      // Thrown currently before update validation.

      std::cout
          << "Cannot change max_threads while any threads are registered\n";
      assert(false);
    }
  }
  active_transactions = vector<epoch_t>(max_threads, MY_NO_TRANSACTION);
  active_read_epochs = vector<epoch_t>(max_threads, MY_NO_TRANSACTION);
  thread_id_in_use = vector<bool>(max_threads, false);
}

// TODO: register VB when creating in VBManager
bool VBData::commitVersionBlock() {
  // return true;
  // std::cout << "start commit with ts : " + std::to_string(epoch_) + '\n';
  // auto start_time = std::chrono::high_resolution_clock::now();

  // if (has_trans.test_and_set()) return false;
  // return true;
  int sm = 0, nm = 0, n = 0, kn = 0;
  for (int wt = 0; wt < WRITE_THREAD; wt++) {
    auto tmpn = vb_vector[wt].size();
    n += tmpn;
    for (int i = 0; i < tmpn; i++) {
      // it->Transform();
      this_vb_vector_.push_back(vb_vector[wt][i]);
      // kn += vb_vector[wt][i]->GetVersionNum();
      // sm += vb_vector[wt][i]->GetVersionNum();
    }
  };
  auto size = n * sizeof(VersionBlock);
  version_block_ = (VersionBlock*)malloc(size);

  std::atomic<int> x(0);

// std::ofstream outfile("version_block_addr.txt", std::ios::app);
// if (!outfile) {
//   std::cerr << "Unable to open file\n";
//   assert(false);
// }
// outfile << epoch_ << '\n' << n << '\n';
// for (int i = 0; i < n; i++) {
//   outfile << (version_block_ + i) << '\n';
// }
// outfile.close();
#pragma omp parallel for num_threads(30)
  for (int i = 0; i < n; i++) {
    new (((VersionBlock*)(version_block_) + i))
        VersionBlock(this_vb_vector_[i]);
    this_vb_vector_[i]->edge_block_->Transform(
        this_vb_vector_[i], ((VersionBlock*)(version_block_) + i));
  }

  // free(tmp);

  // auto end = std::chrono::high_resolution_clock::now();
  // auto duration =
  //     std::chrono::duration_cast<std::chrono::milliseconds>(end -
  //     start_time);

  // std::cout << "epoch " << epoch_ << " " << sm << " " << n << " " << kn << "
  // "
  //           << x << " ms : ";
  // // count_ll = 0;
  // if (duration.count() != 0)
  //   std::cout << " Here Waiting Elapsed time: " + to_string(duration.count())
  //   +
  //                    " ms\n\n";
  return true;
}

AllVBManager::AllVBManager(unsigned max_threads, bool on) : cur_(1) {
  active_txns_[1] = new VBData(1);
  std::cout << "vbdata[1]: " << active_txns_[1] << '\n';
  read_epoch_ = 0;
  read_epoch_cur_ = 1;

  reset_max_threads(max_threads);
  stopped.store(false);
  if (on)
    min_version_updater = thread(&AllVBManager::run_min_epoch_updater, this,
                                 MIN_VERSION_UPDATER_INTERVAL);
  // pool_ = new boost::asio::thread_pool(8);

  // Constructor implementation
}

AllVBManager::~AllVBManager() {
  // Destructor implementation

  stopped.store(true);
  min_version_updater.join();
}

uint64_t AllVBManager::test1() {
  // bool f = false;
  // unsigned times = 0;
  // auto start = std::chrono::high_resolution_clock::now();
  // // 执行你的代码，比如延迟1秒
  epoch_t nw = cur_;
  while (nw - read_epoch_ >= max_simul_batch_num) nw = cur_;
  return nw;
  // std::cout << to_string(cur_ & 0xffffffff) + " " + to_string(read_epoch_) +
  //                  '\n';
  // std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // timess++;
  //   f = true, times++;
  // // 结束时间点
  // if (f) {
  //   auto end = std::chrono::high_resolution_clock::now();
  //   // 计算时间差
  //   auto duration =
  //       std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  //   // 输出经过的时间，单位为ms
  //   std::cout << "Find " + to_string(times) +
  //                    " times with Waiting Elapsed time: " +
  //                    to_string(duration.count()) + " ms\n\n";
  // } else {
  //   auto end = std::chrono::high_resolution_clock::now();
  //   // 计算时间差
  //   auto duration =
  //       std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  //   if (duration.count() != 0)
  //     std::cout << " Here Waiting Elapsed time: " +
  //                      to_string(duration.count()) + " ms\n\n";
  // }
}
// has been locked by TxnManager
uint64_t x = 0;
void AllVBManager::registerTransaction(Transaction* txn) {
  // return;
  // TODO: using the max_simul_batch_num
  // assert(!txn->is_read_only());

  // #ifdef SPIN_LOCK
  //   std::unique_lock<RWSpinLock> lock(mtx_);
  // #else
  //   std::unique_lock<std::mutex> lock(mtx_);
  // #endif
  volatile uint64_t t;
  volatile unsigned cur, active_id;
  VBData* it;
  // t = cur_;
  // cur = t & 0xffffffff;
  // unsigned active_id = t >> 32;
  // while ((cur_ & 0xffffffff) - read_epoch_ > max_simul_batch_num);
  unsigned txn_num = 0;
  do {
    cur = test1();
    active_transactions[thread_id] = cur;
  } while (cur != cur_);

  active_id = cur & MODPM;
  it = active_txns_[active_id];
  // Add the transaction to the current VB
  int x = XorShiftRandom::rand_int(BATCHTXNNUM);
  // break;
  // txn_num = it->add_txn();

  // unsigned txn_num = 1;
  // return;
  // if (txn_num > max_txn_num_invb_) {
  //   active_transactions[thread_id] = numeric_limits<epoch_t>::max();
  //   continue;
  // }
  // std::cout << "register " + to_string(cur) + " " + to_string(txn_num) +
  // '\n';
  // if (txn_num == max_txn_num_git pull github main --allow-unrelated-historiesinvb_) {
  if (cur_ - read_epoch_ < max_simul_batch_num && !x) {
    int req = 0;
    if (is_create.compare_exchange_strong(req, 1)) {
      // auto start = std::chrono::high_resolution_clock::now();
      // 执行你的代码，比如延迟1秒
      if (cur == cur_) {
        unsigned next_id = (active_id + 1) & MODPM;
        // it->set_all_flag();
        // std::cout << next_id << " A\n";
        // std::cout << "new VBData* " + to_string(cur) + '\n';
        active_txns_[next_id] = new VBData(cur + 1);  // new VBData(cur + 1);
        // std::cout << next_id << " B\n";
        cur_++;
        // std::cout << std::to_string(cur_) << " start\n";
        // 结束时间点
        // auto end = std::chrono::high_resolution_clock::now();
        // // 计算时间差
        // auto duration =
        //     std::chrono::duration_cast<std::chrono::milliseconds>(end -
        //     start);
        // // 输出经过的时间，单位为ms
        // std::cout << " new VBData* " + to_string(cur) +
        //                  " Elapsed time: " + to_string(duration.count()) +
        //                  "ms\n\n";
      }
      is_create = 0;
    }
  }
  txn->set_read_epoch(read_epoch_);
  txn->set_epoch(cur);
  txn->set_vb_data(it);
  // std::cout << "txn.epoch: " << txn->get_epoch() << " "
  //           << "txn.vbdata: " << txn->get_vb_data() << '\n';
}

void AllVBManager::registerROTransaction(Transaction* txn, int thread_id_) {
  if (thread_id_ == -1) thread_id_ = thread_id;
  epoch_t re;
  // std::cout << read_epoch_ << " " << min_read_epoch << '\n';
  // exit(0);
  do {
    re = read_epoch_;
    active_read_epochs[thread_id_] = re;
  } while (re < min_read_epoch);
  txn->set_read_epoch(re);
}
void AllVBManager::deregisterROTransaction(int thread_id_) {
  if (thread_id_ == -1) thread_id_ = thread_id;
  active_read_epochs[thread_id_] = MY_NO_TRANSACTION;
}

// has been locked by TxnManager
void AllVBManager::deregisterTransaction() {
  // std::cout << "VBData " + std::to_string(t) +
  //                  " has txn_num : " + std::to_string(it->get_txn_size()) +
  //                  '\n';
  // epoch_t txn_cur = txn->get_epoch();
  active_transactions[thread_id] = MY_NO_TRANSACTION;
  // auto vbd_id = txn_cur % (max_simul_batch_num + 1);

  // auto vbdata = active_txns_[txn_cur % (max_simul_batch_num + 1)];

  // auto num = vbdata->deregisterTransaction(txn);

  // if (num == BATCHTXNNUM - 1)
  //   std::cout << to_string(cur_ & 0xffffffff) + " " +
  //                    to_string(read_epoch_ + 1) + " " + to_string(txn_cur)
  //                    +
  //                    '\n';
  // if (txn_cur == read_epoch_ + 1 && num == BATCHTXNNUM - 1)
  //   commitVersionBlock(0, vbdata);
}
void AllVBManager::commitVersionBlock(epoch_t cur) {
  // std::cout << to_string(cur) + "A" + to_string(read_epoch_ + 1) + " " +
  //                  to_string(vbdata->all_commit()) + '\n';
  // if (read_epoch_ != 0) assert(false);
  // std::cout << "read_epoch_cur_: " << read_epoch_cur_ << " "
  //           << active_txns_[read_epoch_cur_] << '\n';
  auto vbdata = active_txns_[read_epoch_cur_];
  // std::cout << "commit " + to_string(cur) + " " + to_string(read_epoch_) +
  // '\n';
  while (read_epoch_ + 1 < cur) {
    // std::cout << "here " + to_string(read_epoch_) + '\n';
    // assert(false);
    // vbdata->lock_versionblock();
    vbdata->commitVersionBlock();
    // std::cout << "commit " + to_string((uint64_t)vbdata) + " " +
    //                  to_string(read_epoch_) + '\n';

    // delete vbdata;
    vb_que_.push(vbdata);
    // std::cout << "here2 " + to_string(cur) + " " + to_string(read_epoch_) +
    //                  '\n';
    // std::cout << "before: " << read_epoch_ << "  ";
    read_epoch_++;
    read_epoch_cur_ =
        read_epoch_cur_ == max_simul_batch_num - 1 ? 0 : read_epoch_cur_ + 1;

    // std::cout << "after: " << read_epoch_ << '\n';
    // if (cur == read_epoch_ + 1) cur = cur_ & 0xffffffff;
    // std::cout << "here3 " + to_string(cur) + " " + to_string(read_epoch_) +
    //                  '\n';

    // std::cout << "read_epoch_cur_: " << read_epoch_cur_ << " "
    //           << active_txns_[read_epoch_cur_] << '\n';
    vbdata = active_txns_[read_epoch_cur_];
    // std::cout << "here4 " + to_string(cur) + " " +
    //                  to_string((read_epoch_ + 1)) + " " +
    //                  to_string(vbdata->all_commit()) + '\n';
  }
}

void AllVBManager::NoMoreTxn() {
  // #ifdef SPIN_LOCK
  //   std::unique_lock<RWSpinLock> lock(mtx_);
  // #else
  //   std::unique_lock<std::mutex> lock(mtx_);
  // #endif
  no_more_txn_ = true;
  // commitVersionBlock(cur_ & 0xffffffff);
}