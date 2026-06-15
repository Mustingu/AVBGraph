#include <iostream>
#include <thread>
#include <tuple>

#include "CSRBaseline.h"
#include "GraphAlgorithm.h"
#include "MyEdgeArray.h"
#include "MyTime.h"
#include "Permute.h"
#include "VersionBlock/AllVBManager.h"
#include "tbb/concurrent_hash_map.h"
#include "utils/EdgeReader.h"
#include "utils/Interface.h"
#include "utils/Parse.h"
#include "utils/utils.h"

// BUG: too many txn-block will caused sink and transform error

using vertex_dictionary_t = tbb::concurrent_hash_map<uint64_t, vertex_id_t>;
#define VertexDictionary reinterpret_cast<vertex_dictionary_t*>(m_pHashMap)

#define MAXN 200000
#define AVG_EDGE 128

#if DEBUG == false
unsigned n = MAXN;
#else
unsigned n = 10;
#endif

Args Arg;

void* m_pHashMap;

unsigned property_size = 8;

atomic<int> count_x(0), count_y(0), count_vb(0);
atomic<long long> count_ll(0);
MemoryAllocator* la;
thread_local int threadid;
int count_vb_n = 0;


std::map<std::pair<dst_t, dst_t>, std::vector<unsigned>> edge_wight_versioned;
unsigned ans[MAXN + 10];

std::map<std::pair<dst_t, dst_t>, double> edge_wight;
std::map<unsigned, std::tuple<unsigned, dst_t*, dst_t*>> vid2edgewithpro;
std::vector<std::tuple<unsigned, unsigned, uint64_t>> update_tuple;
std::map<std::pair<dst_t, dst_t>, std::pair<unsigned, unsigned>>
    edge_wight_times;

double CheckSumByEdgeWight() {
  double sm = 0;
  for (auto it : edge_wight) {
    sm += it.second;
  }
  return sm;
}

int nm_dota = 0;
class MyTest {
 public:
  AllVBManager* VBM;

  MyEdgeArray* MEA;

  MyTest() {
    VBM = new AllVBManager(65, true);
    la = new MemoryAllocator();
    la->init(50, 65);
    // for (int i = 0; i < 40; i++) {
    //   auto threadt = 1;  // i % 20;
    //   auto it = la->Allocate(8388608, threadt);
    //   std::cout << threadt << ":" << it << '\n';
    // }
    // // std::cout << "init end\n";
    // exit(0);
    MEA = new MyEdgeArray(200);
  }
  unsigned CheckReadEpoch() { return VBM->getCurrentReadEpoch(); }
  double check_avg_degree() {
    double sm = 0;
    std::cout << MEA->node_num << '\n';
    // MEA->check_degree(0);
    for (int i = 0; i < MEA->node_num; i++) {
      // std::cout << i << " : " << MEA->blocks[i] << '\n';
      sm += MEA->check_degree(i);
    }
    // std::cout << "node num : " << nm_dota << '\n';
    std::cout << "sum : " << sm << '\n';
    return sm / MEA->node_num;
  }
  double CheckSum(Transaction* tx, int node_num = n) {
    VersionBlock* vb;

    double sm = 0;

    std::map<std::pair<dst_t, dst_t>, double> output_e;

    for (int i = 0; i < node_num; i++) {
      auto ds =
          dynamic_cast<MyEdgeArray*>(tx->getTopology())->GetBlockByIndex(i);

      ds->getReadLock();

      // std::cout << "   " << i << " : \n\n";

      GraphAlgorithms::for_each_edge_with_property(
          ds, tx->get_read_epoch(), [&](EdgeWithIndex* edge, EdgeWithIndex* p) {
            // printf("%d[%d] %d[%d] : %lf\n", i, MEA->p_mHashMap[i], edge->e,
            //        MEA->p_mHashMap[edge->e],
            //        *reinterpret_cast<double*>(&p->properties));
            // output_e[std::make_pair(MEA->p_mHashMap[i],
            //                         MEA->p_mHashMap[edge->e])] =
            //     *reinterpret_cast<double*>(&p->properties);
            sm += *reinterpret_cast<double*>(&p->properties);
          });
      //       bool need_iterator = false;
      //       auto ts = tx->get_read_epoch();
      //       // std::cout << "ds->getSum start!\n";
      //       double tmp = ds->getSum(i, edge_wight_versioned, ts,
      //       need_iterator, vb,
      //                               MEA->p_mHashMap);

      //       // std::cout << "need_iterator start! meanwhile sm = " << sm <<
      //       // "\n";
      //       // continue;
      //       if (need_iterator) {
      //         // std::cout << "\nyes : " << vb.get() << '\n';
      //         auto it = new VBIterator(vb, ts);
      //         while (!it->is_end_) {
      //           // std::cout << "in while : " << it->vb_.get() << '\n';
      //           auto start = it->vb_->start_;
      //           auto n = it->vb_->version_num_;
      //           for (int i = 0; i < n && start->link.block <= ts; i++) {
      //             // std::cout << (dst_t*)start << " " << (dst_t*)end <<
      //             // '\n';
      // #if MID == true
      //             std::cout << i << " " << *(dst_t*)start << " in " <<
      //             it->vb_
      //                       << " : " << *(dst_t*)(start + 8) << " " << sm <<
      //                       '\n';
      // #endif
      //             // auto os = MEA->p_mHashMap[i],
      //             //      od = MEA->p_mHashMap[start->e & ~DELETION_MASK];
      //             // auto it =
      //             //     os < od ? std::make_pair<>(os, od) :
      //             std::make_pair<>(od,
      //             //     os);
      //             // if (edge_wight_versioned.find(it) ==
      //             edge_wight_versioned.end())
      //             //   std::cout << "there is not in edge_wight_versioned in
      //             VB: "
      //             //             << it.first << " " << it.second << " "
      //             //             <<
      //             (*reinterpret_cast<double*>(&start->properties))
      //             //             << '\n';
      //             tmp += (*reinterpret_cast<double*>(&start->properties));
      // #if DEBUG == true
      //             edge_wight_versioned[std::make_pair(i,
      //             *(dst_t*)start)].push_back(
      //                 *(dst_t*)(start + sizeof(dst_t)));
      // #endif
      //           }

      //           it->getNext();
      //         }
      //         delete it;
      //       }

      ds->unleashReadLock();

      // sm += tmp;
    }
#if MID == true
    unsigned sm2 = 0;
    for (auto it : edge_wight_versioned) {
      for (auto it2 : it.second) {
        sm2 += it2;
        if (edge_wight[it.first] != it2)
          std::cout << "error with : " << it.first.first << " "
                    << it.first.second << " " << it2 << " "
                    << edge_wight[it.first] << "\n";
      }
      if (it.second.size() > 1) {
        std::cout << "here has 2 element in 1 vector!\n";
        std::cout << "   " << it.first.first << " " << it.first.second << " : ";
        for (auto it2 : it.second) std::cout << it2 << " ";
        std::cout << '\n';
      }
    }
#endif

#if MID == true
    for (auto it : edge_wight) {
      if (edge_wight_versioned.find(it.first) == edge_wight_versioned.end()) {
        std::cout << "error2 with : " << it.first.first << " "
                  << it.first.second << " " << it.second
                  << " with edge_wight_times : "
                  << edge_wight_times[it.first].first << " "
                  << edge_wight_times[it.first].second << "\n";
      }
    }
    std::cout << "edge_wight_versioned sm is : " << sm2 << '\n';
#endif

    // for (auto it : output_e) {
    //   printf("%d[%d] %d[%d] : %lf\n", it.first.first,
    //          GetVertexID(it.first.first), it.first.second,
    //          GetVertexID(it.first.second), it.second);
    // }

    return sm;
  }

  MyEdgeBlock* BuildEB(unsigned vid, unsigned n, unsigned m) {
    //  std::cout << i << " " << properties[i + v] << '\n';
    // std::cout << "real answer is : " << sm << '\n';
    auto eb = new MyEdgeBlock(vid);
    eb->build(m, std::get<1>(vid2edgewithpro[vid]),
              std::get<2>(vid2edgewithpro[vid]));
    return eb;
  }
  void BuildAllEB(int type = 0) {
    // if (type == 1) {
    //   for (int i = 0; i <= nm_dota; i++) {
    //     MEA->SetBlockByIndex(i, BuildEB(i, n, 0));
    //   }
    //   return;
    // }
    // for (int i = 0; i <= nm_dota; i++) {
    //   MEA->SetBlockByIndex(i, BuildEB(i, n,
    //   std::get<0>(vid2edgewithpro[i])));
    // }
  }

  template <typename T>
  void grow_vector_if_smaller(tbb::concurrent_vector<T>& v, size_t s,
                              T init_value) {
    if (v.size() <=
        s) {  // Only synchronize with other threads if potentially necessary
      std::scoped_lock<std::mutex> l(MEA->growing_vector_mutex);
      if (v.size() <= s) {
        MEA->p_mHashMap.resize(v.capacity());
        v.grow_to_at_least(v.capacity() * 2);
      }
    }
  }

  dst_t GetVertexID(dst_t src) {
    dst_t p_id;
    vertex_dictionary_t::accessor w;
    if (VertexDictionary->insert(w, src)) {
      auto eb = new MyEdgeBlock();
      eb->build(0, nullptr, nullptr);
      // p_id = MEA->new_vertex();

      p_id = MEA->node_num.fetch_add(1);
      grow_vector_if_smaller(MEA->blocks, p_id, MEBC());
      // MEA->growing_vector_mutex.lock();
      // auto it = MEA->blocks.push_back(MEBC(eb));
      MEA->p_mHashMap[p_id] = src;
      MEA->blocks[p_id].eb = eb;

      eb->setSrc(p_id);
      // MEA->growing_vector_mutex.unlock();

      w->second = p_id;
      // aquire_vertex_lock_p(p_id);

      // Update index
      //  assert(index[p_id].adjacency_set & VERTEX_NOT_USED_MASK);

      w.release();

    } else {
      p_id = w->second;
      w.release();
    }
    return p_id;
  }

  void UpdateEB(int i) {
    dst_t f[3], g[3];
    f[0] = std::get<0>(update_tuple[i]);
    f[1] = std::get<1>(update_tuple[i]);
    f[2] = std::get<2>(update_tuple[i]);
    g[0] = std::get<1>(update_tuple[i]);
    g[1] = std::get<0>(update_tuple[i]);
    g[2] = std::get<2>(update_tuple[i]);

    // uint64_t internal_source_id = numeric_limits<uint64_t>::max();
    // uint64_t internal_destination_id = numeric_limits<uint64_t>::max();

    // bool insert_source = false;
    // bool insert_destination = false;
    // vertex_dictionary_t::const_accessor slock1, slock2;
    // vertex_dictionary_t::accessor xlock1, xlock2;
    // bool result = false;
    // if (VertexDictionary->find(slock1, f[0])) {  // insert the vertex
    // e.m_source
    //   internal_source_id = slock1->second;
    // } else {
    //   slock1.release();
    //   if (VertexDictionary->insert(xlock1, f[0])) {
    //     internal_source_id = xlock1->second = MEA->new_vertex();
    //   } else {
    //     internal_source_id = xlock1->second;
    //   }
    // }

    // if (VertexDictionary->find(slock2,
    //                            f[1])) {  // insert the vertex
    //                            e.m_destination
    //   internal_destination_id = slock2->second;
    // } else {
    //   slock2.release();
    //   if (VertexDictionary->insert(xlock2, f[1])) {
    //     internal_destination_id = xlock2->second = MEA->new_vertex();
    //   } else {
    //     internal_destination_id = xlock2->second;
    //   }
    // }

    if (Arg.read_test && Arg.check_) {
      auto it = f[0] < f[1] ? std::make_pair<>(f[0], f[1])
                            : std::make_pair<>(f[1], f[0]);
      edge_wight_versioned[it].push_back(*(reinterpret_cast<double*>(f + 2)));
    }

    f[0] = g[1] = GetVertexID(f[0]);
    f[1] = g[0] = GetVertexID(f[1]);

    // return;
    // std::cout << to_string(internal_source_id) + " " +
    //                  to_string(internal_destination_id) + " "
    //           << MEA->blocks[0] << '\n';
    // return;

    Transaction txn = Transaction(0, false, true, MEA);
    VBM->registerTransaction(&txn);

    txn.insertedge(f);
    txn.insertedge(g);

    // txn.INSERT(f);
    // txn.INSERT(g);
    // txn.execute();
    txn.COMMIT();

    VBM->deregisterTransaction();

#if DEBUG == true
    if (edge_wight.find(std::make_pair(src, dst)) != edge_wight.end())
      //   // std::cout << src << " -> " << dst
      //   //           << " with old value : " <<
      //   edge_wight[std::make_pair(src,
      //   //           dst)]
      //   //           << " and new value : " << t << '\n';
      ans[src] -= edge_wight[std::make_pair(src, dst)];
    edge_wight[std::make_pair(src, dst)] = t;
    ans[src] += t;
#if MID == true
    if (edge_wight_times.find(std::make_pair(src, dst)) ==
        edge_wight_times.end()) {
      edge_wight_times[std::make_pair(src, dst)] = std::make_pair(0, 1);
    } else {
      edge_wight_times[std::make_pair(src, dst)].second++;
    }
#endif
#endif
  }
} mytest;
void Initial_Data() {
  int n = 100;
  double sm = 0;
  for (int i = 0; i < n; i++) {
    unsigned vid = i;
    unsigned m = rand() % (int)(0.1 * n) + (int)(0.05 * n);
    unsigned property_size = 8;
    dst_t* edges = new dst_t[m + 1];
    uint64_t* properties = new uint64_t[m + 1];
    std::vector<bool> v(n, false);
    for (int k = 0; k < m; k++) {
      int j;
      do {
        j = std::rand() % n;
      } while (v[j] || j == vid);
      v[j] = true;
      edges[k] = j;
    }
    sort(edges, edges + m);
    unsigned t = 0;
    for (int i = 0; i < m; i++) {
      properties[i] = std::rand() % 100 + 1;
      update_tuple.push_back(std::make_tuple(vid, edges[i], properties[i]));
#if DEBUG == true
      edge_wight[std::make_pair(vid, edges[i])] = properties[i];
      edge_wight_times[std::make_pair(vid, edges[i])] = std::make_pair(1, 0);
#endif
      t += properties[i];
    }
    vid2edgewithpro[vid] = std::make_tuple(m, edges, properties);
    ans[vid] = t;
    sm += t;
  }
}

#if DEBUG == true
unsigned times = n * BATCHTXNNUM;
#else
unsigned times = n * AVG_EDGE;
#endif
unsigned write_thread_num = 20;
bool Has_read_thread = false;
bool constant_time = false;

std::atomic<int> w_cnt, r_cnt;

std::map<int, int> X;
int Max = 0;

void Mytest_Write_Thread(int st, int ed, int thread_id) {
  // std::cout << st << " " << ed << " " << thread_id << '\n';
  for (int i = st; i < ed; i++) {
    // mytest.mtx.lock();
    mytest.UpdateEB(i);
    // w_cnt++;
    // mytest.mtx.unlock();
  }
}
void Mytest_Read_Thread() {
  while (1) {
    // mytest.mtx.lock();
    Transaction* txn = new Transaction(1, true, false, mytest.MEA);
    txn->set_read_epoch(mytest.VBM->getCurrentReadEpoch());
    // edge_wight_versioned.clear();
    int xx = mytest.CheckSum(txn);
    // std::cout << txn->get_read_epoch() << '\n';
    r_cnt++;
    // mytest.mtx.unlock();
  }
  std::cout << "EndQuery\n";
}
void SleepingThread() {
  for (int i = 0; i < 5; i++) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Sleep for " + to_string(i + 1) + " seconds\n";
    std::cout << "w_cnt = " << w_cnt << ", r_cnt = " << r_cnt << '\n';
    std::cout << "count_x = " << count_x << ", count_y = " << count_y << '\n';
  }
  std::cout << "count_vb = " << count_vb << " "
            << "average num = " << (double)(w_cnt) / count_vb << '\n';
  exit(0);
  return;
}
void Txn_Write() {
  std::vector<std::thread> v;
  std::thread t3;
  std::thread t2;
  for (int i = 0; i < write_thread_num; i++)
    if (i != write_thread_num - 1) {
      v.emplace_back(Mytest_Write_Thread, i * (times / write_thread_num) + 1,
                     (i + 1) * (times / write_thread_num) + 1, i);
    } else {
      v.emplace_back(Mytest_Write_Thread, i * (times / write_thread_num) + 1,
                     times + 1, i);
    }

  if (constant_time) t3 = std::thread(SleepingThread);
  if (Has_read_thread) {
    t2 = std::thread(Mytest_Read_Thread);
    t2.join();
  }
  for (int i = 0; i < write_thread_num; i++) {
    v[i].join();
  }

  std::cout << "write_end\n";
  mytest.VBM->NoMoreTxn();
  delete mytest.VBM;

  mytest.VBM->P_timess();
  std::cout << "count_x = " << count_x << ", count_y = " << count_y << '\n'
            << "count_vb = " << count_vb << '\n';
  return;
  MyEdgeArray* MEA_t = mytest.MEA;
  for (int i = 1; i <= times; i++) {
    mytest.UpdateEB(i);
    if (i % BATCHTXNNUM == 0) {
      unsigned sm = 0;
#if DEBUG == true
      for (int i = 0; i < n; i++) sm += ans[i];
      std::cout << i << " : " << sm
                << " with real answer : " << CheckSumByEdgeWight() << '\n';
#endif

      for (int j = 0; j < READ_TIMES; j++) {
        Transaction* txn = new Transaction(1, true, false, MEA_t);
        txn->set_read_epoch(mytest.VBM->getCurrentReadEpoch());

        // edge_wight_versioned.clear();
        int xx = mytest.CheckSum(txn);

#if OUTPUT == true
        std::cout << "sum before update1 : " << xx
                  << " with read epoch : " << txn->get_read_epoch() << '\n';
        std::cout << (int)(xx - sm) << '\n';
        std::cout << "\n\n";
#endif
      }
    }
  }
}

void Mytest_Debug() {
  MyEdgeArray* MEA_t = mytest.MEA;
  std::cout << "real answer : " << CheckSumByEdgeWight() << '\n';
  Transaction* txn = new Transaction(1, true, false, MEA_t);
  txn->set_read_epoch(mytest.VBM->getCurrentReadEpoch());

  int xx = mytest.CheckSum(txn);

#if OUTPUT == true
  std::cout << "sum before update1 : " << xx
            << " with read epoch : " << txn->get_read_epoch() << '\n';
  std::cout << "\n\n";
#endif

  int t = 0;
  for (int i = 1; i <= times; i++) {
    mytest.UpdateEB(i);
    if (i % BATCHTXNNUM == 0) {
      std::cout << "i = " << i << '\n';
      t++;
      while (mytest.CheckReadEpoch() != t);
      // mytest.MEA->Print(5);
      unsigned sm = 0;
#if DEBUG == true
      for (int i = 0; i < n; i++) sm += ans[i];
      std::cout << i << " : " << sm
                << " with real answer : " << CheckSumByEdgeWight() << '\n';
      edge_wight_versioned.clear();
#endif
      for (int j = 0; j < READ_TIMES; j++) {
        Transaction* txn = new Transaction(1, true, false, MEA_t);
        txn->set_read_epoch(mytest.VBM->getCurrentReadEpoch());

        int xx = mytest.CheckSum(txn);

#if OUTPUT == true
        std::cout << "sum before update1 : " << xx
                  << " with read epoch : " << txn->get_read_epoch() << '\n';
        std::cout << (int)(xx - sm) << '\n';
        std::cout << "\n\n";
#endif
      }
    }
  }
}

std::map<unsigned, std::set<unsigned>> ct;
std::map<unsigned, unsigned> trans;

int Spruce_wt = 64;
void WriteLikeSpruce(bool is_new = true) {
  times = update_tuple.size();

  std::vector<std::thread> v;

  atomic<uint64_t> start_chunk_next(0);
  mytest.VBM->start_count = true;
  uint64_t m_scheduler_granularity = 1 << 10;
  for (int i = 0; i < Spruce_wt; i++) {
    v.emplace_back(
        [&](int thread_id) {
          threadid = thread_id;
          if (is_new) std::cout << "register thread " << thread_id << '\n';
          mytest.VBM->register_thread(thread_id, is_new);
          uint64_t start;
          while ((start = start_chunk_next.fetch_add(m_scheduler_granularity)) <
                 times) {
            uint64_t end =
                std::min<uint64_t>(start + m_scheduler_granularity, times);
            // run_sequential(interface, graph, start, end);
            Mytest_Write_Thread(start, end, thread_id);
          }
          // std::cout << "Thread " + to_string(thread_id) + " finished\n";
        },
        static_cast<int>(i));
    // if (i != Spruce_wt - 1) {
    //   v.emplace_back(Mytest_Write_Thread, i * (times / Spruce_wt) + 1,
    //                  (i + 1) * (times / Spruce_wt) + 1, i);
    // } else {
    //   v.emplace_back(Mytest_Write_Thread, i * (times / Spruce_wt) + 1,
    //                  times + 1, i);
    // }
  }
  for (int i = 0; i < Spruce_wt; i++) {
    v[i].join();
  }
  std::cout << "write finish\n" << std::endl;
  // if (is_new) mytest.VBM->NoMoreTxn();
  // while (1);
  // for (int i = 0; i < mytest.MEA->node_num; i++)
  //   // if (mytest.MEA->blocks[i] == 0)
  //   std::cout << i << ":" << mytest.MEA->blocks[i] << "\n";
  // Mytest_Write_Thread(1, n * AVG_EDGE + 1, 1);
}

void GetSum() {
  Transaction* txn = new Transaction(1, true, false, mytest.MEA);
  mytest.VBM->registerROTransaction(txn);
  std::cout << "\nread txn epoch : " << txn->get_read_epoch() << '\n';
  // edge_wight_versioned.clear();
  double xx = mytest.CheckSum(txn, VertexDictionary->size());
  mytest.VBM->deregisterROTransaction();
  std::cout << "All edge weight sum : " << xx << '\n';

  if (Arg.check_)
    std::cout << "All edge weight sum with map : " << CheckSumByEdgeWight() * 2
              << "\n\n\n";
}
void UpdateData() {
  if (Arg.IsFileInput()) {
    std::ifstream in(Arg.input_file);
    if (!in.is_open()) {
      std::cerr << "open file failed." << std::endl;
      return;
    }
    std::string line;
    times = 0;
    int src_num = 0;
    while (std::getline(in, line)) {
      ++times;
      uint64_t src, dst, property;
      sscanf(line.c_str(), "%lu %lu %lf", &src, &dst, (double*)&property);
      trans[src]++;
      update_tuple.push_back(std::make_tuple(src, dst, property));
      if (Arg.check_) {
        if (src > dst) swap(src, dst);
        auto x = std::make_pair(src, dst);
        edge_wight[x] = *reinterpret_cast<double*>(&property);
      }
      if (Arg.TouchLimitEdgeNum(times)) break;
    }
    std::cout << times << '\n';
    in.close();

    if (Arg.permute_) {
      permute(times, update_tuple);
    }
    std::cout << "Has edges number : " << times << '\n';
  }
}

void DoAlgorithm() {
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if (Arg.algorithm_ == "sum") {
    Transaction* txn = new Transaction(1, true, false, mytest.MEA);
    mytest.VBM->registerROTransaction(txn);
    std::cout << "read txn epoch : " << txn->get_read_epoch() << '\n';
    // edge_wight_versioned.clear();
    double xx = mytest.CheckSum(txn, VertexDictionary->size());
    std::cout << "All edge weight sum : " << xx << '\n';
    if (Arg.check_)
      std::cout << "All edge weight sum with map : "
                << CheckSumByEdgeWight() * 2 << '\n';
  }

  // if (Arg.algorithm_ == "pagerank" || Arg.algorithm_ == "pr") {
  PageRank pr(mytest.MEA, mytest.VBM, Arg.read_thread);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  PrintFunctionTime(
      [&]() {
        omp_set_num_threads(64);
        pr.compute_pagerank(10, 0.85 /*, mytest.MEA*/);
      },
      "PageRank");
  // }

  // if (Arg.algorithm_ == "bfs") {
  BFS bfs(mytest.MEA, mytest.VBM, Arg.read_thread);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  PrintFunctionTime(
      [&]() {
        omp_set_num_threads(64);
        bfs.bfs(mytest.GetVertexID(Arg.bfsroot), 15, 18 /*, mytest.MEA*/);
      },
      "BFS");
  // }

  // if (Arg.algorithm_ == "sssp") {
  SSSP sssp(mytest.MEA, mytest.VBM, Arg.read_thread);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  PrintFunctionTime(
      [&]() {
        omp_set_num_threads(64);
        sssp.compute_sssp(mytest.GetVertexID(Arg.bfsroot), 2 /*, mytest.MEA*/);
      },
      "SSSP");
  // }
}
void OutputData() {
  std::cout << "start output data\n";
  std::ofstream out("output.e");
  if (!out.is_open()) {
    std::cerr << "open file failed." << std::endl;
    return;
  }
  for (const auto& p : update_tuple) {
    out << std::get<0>(p) << " " << std::get<1>(p) << " " << std::get<2>(p)
        << '\n';
  }
}

int timee = 0;
bool GetOneBatch(std::ifstream& in, int& remain) {
  std::string line;
  times = 0;

  update_tuple.clear();
  while (std::getline(in, line)) {
    ++times;
    uint64_t src, dst, property;
    sscanf(line.c_str(), "%lu %lu %lf", &src, &dst, (double*)&property);
    trans[src]++;
    update_tuple.push_back(std::make_tuple(src, dst, property));
    if (src > dst) swap(src, dst);
    auto x = std::make_pair(src, dst);

    if (edge_wight.find(x) != edge_wight.end()) {
      auto old = edge_wight[x];
      // if (-((int)(old + 1) / 1000) == 0) std::cout << "?" << old << endl;
      // std::cout << -((int)(old + 1) / 1000) << endl;
      edge_wight[std::make_pair(-((int)(old + 1) / 1000), -1)] +=
          (int)old % 1000;
      // *reinterpret_cast<double*>(&property);
    }
    edge_wight[x] = timee * 1000 + *reinterpret_cast<double*>(&property);
    if (times == remain) return false;
    if (times == BATCHTXNNUM) return (remain -= BATCHTXNNUM), true;
  }
  return false;
}
void ReadTest() {
  if (Arg.check_) {
    assert(Spruce_wt == 1);
  }
  int remaining_num = Arg.limit_edge_nums;
  if (!remaining_num) remaining_num = -1;
  int t = 1;
  Transaction* txn[100];
  double ans[100];
  timee = 1;
  if (Arg.IsFileInput()) {
    std::ifstream in(Arg.input_file);
    if (!in.is_open()) {
      std::cerr << "[Error] open file failed." << std::endl;
      return;
    }
    while (GetOneBatch(in, remaining_num)) {
      WriteLikeSpruce(false);

      while (mytest.VBM->getCurrentReadEpoch() != t) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      if (Arg.check_) {
        double sm = 0;  //, tt = edge_wight[std::make_pair(-1, -1)];
        // edge_wight[std::make_pair(-1, -1)] = 0;
        for (auto it : edge_wight)
          if (it.first.second != -1) {
            sm += (int)it.second % 1000;
          }
        // std::cout << "right sum : " << sm * 2 << " " << tt * 2 << " "
        //           << sm * 2 - tt * 2 << '\n';
        ans[timee++] = sm * 2;
      }

      txn[t] = new Transaction(1, true, false, mytest.MEA);
      mytest.VBM->registerROTransaction(txn[t], t);
      // mytest.VBM->deregisterROTransaction(t);
      // edge_wight_versioned.clear();
      t++;

      // GetSum();
      if (remaining_num == 0) return;
    }

    if (remaining_num == 0) return;
    int ttt = 0;
    if (Arg.check_) {
      double sm = 0;  //, tt = edge_wight[std::make_pair(-1, -1)];
      // edge_wight[std::make_pair(-1, -1)] = 0;
      for (auto it : edge_wight)
        if (it.first.second != -1) {
          sm += (int)it.second % 1000;
          ttt++;
        }
      std::cout << ttt << endl;
      // std::cout << "right sum : " << sm * 2 << " " << tt * 2 << " "
      //           << sm * 2 - tt * 2 << '\n';
      ans[timee++] = sm * 2;
    }

    WriteLikeSpruce(false);
    mytest.VBM->NoMoreTxn();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // GetSum();

    txn[t] = new Transaction(1, true, false, mytest.MEA);
    mytest.VBM->registerROTransaction(txn[t], t);
    std::cout << "\nread txn epoch : " << txn[t]->get_read_epoch() << '\n';

    in.close();

    for (int i = 1; i <= t; i++) {
      std::cout << "read_epoch = " << txn[i]->get_read_epoch() << ", sum = "
                << mytest.CheckSum(txn[i], VertexDictionary->size()) << "\n";
      mytest.VBM->deregisterROTransaction(t);

      std::cout << "right sum : " << ans[i] << "\n";
    }
  } else {
    std::cout << "[Error] without input file!!\n";
  }
}

void ConcurrentWRTest() {
  PrintFunctionTime(
      [&]() { UpdateDataConcurrent(Arg.input_file, update_tuple); },
      "Input Edge Data");
  if (Arg.permute_) {
    permute(update_tuple.size(), update_tuple);
  }

  std::vector<PageRank*> PRs;

  if (Arg.single_read_work) {
    PRs.push_back(new PageRank(mytest.MEA, mytest.VBM, Arg.read_thread));
  } else {
    for (int i = 0; i < 1; i++) {
      PRs.push_back(new PageRank(mytest.MEA, mytest.VBM, 5));
    }
  }

  std::atomic<bool> finish(false);
  std::atomic<long long> total_time(0);  // 累积总时间(微秒)
  std::atomic<int> execution_count(0);   // 执行次数统计

  auto start = std::chrono::high_resolution_clock::now();

  WriteLikeSpruce(true);
  std::cout << "first write finish\n";
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();

  std::cout << "finish first write time : " << duration / 1000 << "ms\n\n";
  std::thread write_thread([&finish]() {
    PrintFunctionTime(
        [&]() {
          for (int i = 0; i < 4; i++) WriteLikeSpruce(false);
          finish = true;
        },
        "Write");
  });
  // write_thread.join();
  // return;
  // write_thread.join();
  // return;
  // WriteLikeSpruce(true);
  // auto end = std::chrono::high_resolution_clock::now();
  // auto duration =
  //     std::chrono::duration_cast<std::chrono::microseconds>(end - start)
  //         .count();
  // std::cout << "finish first write time : " << duration / 1000 << "
  // ms\n\n";

  // std::thread write_thread([&finish]() {
  //   auto start = std::chrono::high_resolution_clock::now();
  //   WriteLikeSpruce(false);
  //   mytest.VBM->NoMoreTxn();
  //   auto end = std::chrono::high_resolution_clock::now();
  //   auto duration =
  //       std::chrono::duration_cast<std::chrono::microseconds>(end -
  //       start)
  //           .count();
  //   std::cout << "write time : " << duration / 1000 << " ms\n";
  //   finish = true;
  // });

  int t = Arg.Spruce_wt;

  std::vector<std::future<void>> futures;
  for (int i = 0; i < PRs.size(); i++) {
    auto& pr = PRs[i];

    futures.emplace_back(std::async(
        std::launch::async,
        [&](int thread_id) {
          std::cout << "read thread : " << thread_id << "\n";
          mytest.VBM->register_thread(thread_id, true);
          while (!finish) {
            if (Arg.single_read_work) {
              omp_set_num_threads(Arg.read_thread);
            } else {
              omp_set_num_threads(1);
            }
            auto start = std::chrono::high_resolution_clock::now();
            pr->compute_pagerank(10, 0.85);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start)
                    .count();

            // 累积时间和计数
            total_time += duration;
            execution_count++;
            std::cout << "Average time: " +
                             std::to_string(static_cast<double>(total_time) /
                                            execution_count) +
                             '\n';

            // std::cout << "pr time : " << duration << " us\n";
          }
        },
        (t + i)));
  }

  write_thread.join();

  // for (auto& pr : PRs) {
  //   auto start = std::chrono::high_resolution_clock::now();
  //   pr->compute_pagerank(10, 0.85);
  //   auto end = std::chrono::high_resolution_clock::now();
  //   auto duration =
  //       std::chrono::duration_cast<std::chrono::microseconds>(end -
  //       start)
  //           .count();
  //   std::cout << "pr time : " << duration << " us\n";
  // }

  for (auto& f : futures) {
    f.get();
  }

  // 计算并输出平均时间
  if (execution_count > 0) {
    double avg_time = static_cast<double>(total_time) / execution_count;
    std::cout << "\n=== PageRank Performance Summary ===" << std::endl;
    std::cout << "Total executions: " << execution_count << std::endl;
    std::cout << "Total time: " << total_time << " us" << std::endl;
    std::cout << "Average time: " << avg_time << " us" << std::endl;
    std::cout << "Average time: " << avg_time / 1000.0 << " ms" << std::endl;
  } else {
    std::cout << "No PageRank executions completed." << std::endl;
  }
}
int main(int argc, char** argv) {
  m_pHashMap =
      new tbb::concurrent_hash_map<uint64_t, /* vertex_t */ uint64_t>();
  // TestMemory();
  // return 0;
  Arg.ParseArgs(argc, argv);
  // PrintFunctionTime(UpdateData, "UpdateData");
  // return 0;
  // PrintFunctionTime(
  //     [&]() {  },
  //     "EdgeReader");
  // return 0;
  Spruce_wt = Arg.Spruce_wt;

  if (Arg.gen_and_output) {
    Initial_Data();
    OutputData();
    return 0;
  }

  if (Arg.read_test) {
    ReadTest();
    return 0;
  }

  if (Arg.con_wr_test) {
    ConcurrentWRTest();
    return 0;
  }

  // UpdateData();
  PrintFunctionTime(
      [&]() { UpdateDataConcurrent(Arg.input_file, update_tuple); },
      "Input Edge Data");
  if (Arg.permute_) {
    permute(update_tuple.size(), update_tuple);
  }

  if (Arg.csr) {
    std::cout << "========== Running CSR Baseline Mode ==========" << std::endl;

    // 构建 CSR
    CSRGraph csr_g;
    int n = update_tuple.size();
    for (auto i = 0; i < n; i++) {
      update_tuple.push_back(std::make_tuple(std::get<1>(update_tuple[i]),
                                             std::get<0>(update_tuple[i]),
                                             std::get<2>(update_tuple[i])));
    }
    std::cout << update_tuple.size() << " edges\n";
    csr_g.build(update_tuple);

    CSRBaseline csr_runner(&csr_g);

    // 假设 Arg.source 是你在其他地方定义的源点，或者手动指定
    // 如果 Arg.source 没有在 Initial_Data 中设置，默认取 update_tuple[0] 的源点
    uint64_t source_node = Arg.bfsroot;
    std::cout << omp_get_max_threads() << " threads\n";
    omp_set_num_threads(64);
    PrintFunctionTime([&]() { csr_runner.bfs(source_node); }, "bfs");
    PrintFunctionTime([&]() { csr_runner.sssp(source_node, 2.0); }, "sssp");
    PrintFunctionTime([&]() { csr_runner.pagerank(10, 0.85); }, "pagerank");

    return 0;  // CSR 运行完直接退出，不运行后面的 AVB 逻辑
  }

  std::cout << update_tuple.size() << '\n';
  std::cout << "\nfinish read\n\n";
  if (Arg.algorithm_.empty()) {
    PrintFunctionTime([]() { WriteLikeSpruce(true); }, "mytest.WriteOnly");
  } else {
    PrintFunctionTime(
        []() {
          WriteLikeSpruce(true);
          mytest.VBM->NoMoreTxn();
        },
        "mytest.WriteBeforeAlgorithm");
    DoAlgorithm();
    return 0;
  }

  // if (std::strcmp(argv[1], "--Spruce") == 0) {
  //   if (argc > 2 && atoi(argv[2]) == 1) {
  //     slttest.BuildAllEB(1);
  //     PrintFunctionTime(WriteLikeSpruce2, "slttest.WriteLikeSpruce");
  //     return 0;
  //   } else {
  //     mytest.BuildAllEB(1);
  //     PrintFunctionTime(TestNew, "mytest.testnew");
  //     PrintFunctionTime(WriteLikeSpruce, "mytest.WriteLikeSpruce");
  //     PrintFunctionTime(TestNew, "mytest.testnew");
  //     std::cout << "avg degree after is : " << mytest.check_avg_degree()
  //               << '\n';
  //     std::cout << "count_vb after is : " << count_vb << '\n';
  //     return 0;
  //   }
  // }

  // mytest.BuildAllEB();

  // if (argc > 1) {
  //   if (argc > 2) {
  //     is_slt = atoi(argv[2]);
  //   }
  //   if (is_slt) {
  //     std::cout << "using SLTtest\n";
  //   } else {
  //     std::cout << "using Mytest\n";
  //   }
  //   if (!is_slt) {
  //     if (std::strcmp(argv[1], "--debug") == 0) {
  //       PrintFunctionTime(Mytest_Debug, "Mytest_Debug");
  //       return 0;
  //     }
  //   } else {
  //     if (std::strcmp(argv[1], "--debug") == 0) {
  //       PrintFunctionTime(SLTtest_Debug, "SLTtest_Debug");
  //       return 0;
  //     }
  //   }
  // }
  // if (argc > 1) {
  //   is_slt = atoi(argv[1]);
  // }
  // if (is_slt) {
  //   std::cout << "using SLTtest\n";
  // } else {
  //   std::cout << "using Mytest\n";
  // }

  // auto slt_txn = slttest.GetReadOnly();

  // int xx = slttest.CheckSum(slt_txn);
  // std::cout << "sum before update1 : " << xx
  //           << " with read epoch : " << slt_txn.get_version() << '\n';
  // std::cout << "\n\n";

  // slttest.tm->transactionCompleted(slt_txn);
  // // mytest.BuildAllEB();
  // // // // mytest.PrintFunctionTime(Txn_Write, "Txn_Write");
  // if (is_slt == false) {
  //   std::cout << "avg degree before is : " << mytest.check_avg_degree()
  //   <<
  //   '\n'; PrintFunctionTime(Txn_Write, "mytest.Txn_Write"); std::cout <<
  //   "avg degree after is : " << mytest.check_avg_degree() << '\n';
  // }
  // // SLT_Write();
  // // ReBuild();
  // else {
  //   PrintFunctionTime(SLT_Write, "slttest.SLT_Write");
  // }
  return 0;
}