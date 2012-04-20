// chameneos.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <atomic>
//#include <memory>
#include <vector>
#include <string>
//#include <chrono>

using namespace std;


enum color
{
    unknown = 0,
    blue,
    red,
    yellow
};

const char* color_names[] = { "unknown", "blue", "red", "yellow" };

color color_complement(int c1, int c2)
{
   switch (c1)
   {
   case color::blue:
      switch (c2)
      {
      case color::blue:      return color::blue;
      case color::red:       return color::yellow;
      case color::yellow:    return color::red;
      }
   case color::red:
      switch (c2)
      {
      case color::blue:      return color::yellow;
      case color::red:       return color::red;
      case color::yellow:    return color::blue;
      }
   case color::yellow:
      switch (c2)
      {
      case color::blue:      return color::red;
      case color::red:       return color::blue;
      case color::yellow:    return color::yellow;
      }
   }
   assert(0);
   return color::unknown;
}


void print_colors()
{
    for (int c1 = color::blue; c1 <= color::yellow; ++c1)
        for (int c2 = color::blue; c2 <= color::yellow; ++c2)
        {
            color c3 = color_complement(c1, c2);
            cout << color_names[c1] << " + " << color_names[c2] << " -> " << color_names[c3] << "\n";
        }
    cout << endl;
}

string spell_n(size_t n)
{
    static const char* digits[] = {
        " zero", " one", " two",   " three", " four",
        " five", " six", " seven", " eight", " nine"
    };

    return n < 10 ? digits[n] : spell_n(n/10).append(digits[n%10]);
}


#ifdef __WIN32
#define ALIGNED_DECL __declspec(align(64))
#else
#define ALIGNED_DECL
#endif


ALIGNED_DECL class MeetingPlace
{
public:

    MeetingPlace(unsigned int state) : state_(state) {}
    atomic<unsigned int>& state() { return state_; }

private:

    atomic<unsigned int> state_;
};

static const unsigned int MEET_COUNT_SHIFT = 8;
static const unsigned int CHAMENEOS_IDX_MASK = 0xFF;


ALIGNED_DECL class Chameneos;
typedef unique_ptr<Chameneos> ChameneosPtr;
typedef vector<ChameneosPtr> Team;

ALIGNED_DECL class Chameneos
{
public:

    Chameneos(size_t id, color color, const shared_ptr<MeetingPlace>& place, Team& team)
        : id_(id), color_(color), meet_count_(0), meet_self_count_(0),
          meeting_completed_(false), place_(place), team_(team)
    {}

    void start()
    {
        assert(!thread_);
        thread_.reset(new thread(bind(&Chameneos::run, this)));
    }

    void join()
    {
        assert(thread_);
        thread_->join();
        thread_.reset();
    }

    size_t meet_count() const { return meet_count_; }
    size_t meet_self_count() const { return meet_self_count_; }

private:

    void run();

    const size_t id_;

    color color_;
    size_t meet_count_;
    size_t meet_self_count_;
    bool meeting_completed_;

    shared_ptr<MeetingPlace> place_;
    unique_ptr<thread> thread_;
    Team& team_;
};


void Chameneos::run()
{
    auto& p_state = place_->state();
    unsigned int state = p_state.load(memory_order_relaxed);

    for (;;)
    {
        unsigned int peer_idx = state & CHAMENEOS_IDX_MASK;
        unsigned int xchg;

        if (peer_idx)
            xchg = state - peer_idx - (1 << MEET_COUNT_SHIFT);
        else if (state)
            xchg = state | id_;
        else
            break;

        if (p_state.compare_exchange_weak(state, xchg, memory_order_relaxed))
        {
            if (peer_idx)
            {
                auto& peer = team_[peer_idx - 1];
                color new_color = color_complement(color_, peer->color_);

                peer->color_ = new_color;
                ++peer->meet_count_;

                //std::atomic_thread_fence(memory_order_release);
                peer->meeting_completed_ = true;

                color_ = new_color;
                ++meet_count_;

                if (peer_idx == id_)
                    meet_self_count_ += 2;
            }
            else
            {
                //do { this_thread::yield(); }
                //while (!meeting_completed_);

                size_t spin_count = numeric_limits<size_t>::max() >> 8;
                while (!meeting_completed_)
                {
                    if (spin_count > 0)
                        --spin_count;
                    else
                        this_thread::yield();
                }

                meeting_completed_ = false;
            }
        }
    }
}


void init_and_start(vector<ChameneosPtr>& team, const color* initial_colors, const size_t meet_count)
{
    assert(initial_colors);

    auto place = make_shared<MeetingPlace>(meet_count << MEET_COUNT_SHIFT);

    for (size_t i = 0; initial_colors[i] != color::unknown; ++i)
        team.emplace_back(new Chameneos(i+1, initial_colors[i], place, team));

    for (auto i = team.begin(); i != team.end(); ++i)
        (*i)->start();
}


void join_and_print(vector<ChameneosPtr>& team, const color* initial_colors)
{
    for (auto i = team.begin(); i != team.end(); ++i)
        (*i)->join();

    assert(initial_colors);
    while (*initial_colors != color::unknown)
    {
        cout << " " << color_names[*initial_colors];
        ++initial_colors;
    }
    cout << endl;

    size_t total_meet_count = 0;
    for (auto& c : team)
    {
        cout << c->meet_count() << spell_n(c->meet_self_count()) << endl;
        total_meet_count += c->meet_count();
    }

    cout << spell_n(total_meet_count) << "\n\n" << flush;
}


void run(size_t meet_count)
{
    static const color initial_colors1[] = {
        color::blue, color::red, color::yellow, color::unknown
    };

    static const color initial_colors2[] = {
        color::blue, color::red, color::yellow, color::red, color::yellow,
        color::blue, color::red, color::yellow, color::red, color::blue, color::unknown
    };

    vector<ChameneosPtr> team1;
    vector<ChameneosPtr> team2;

    chrono::system_clock::time_point start = chrono::system_clock::now();

    init_and_start(team1, initial_colors1, meet_count);
    init_and_start(team2, initial_colors2, meet_count);

    join_and_print(team1, initial_colors1);
    join_and_print(team2, initial_colors2);

    chrono::duration<double> elapsed = chrono::system_clock::now() - start;
    cout << "finished in " << elapsed.count() << "s" << endl << endl;
}

int main(int argc, char* argv[])
{
    size_t meet_count = 600;
    if (argc > 1 && atoi(argv[1]) > 0)
        meet_count = atoi(argv[1]);

    print_colors();
    run(meet_count);

    return 0;
}

