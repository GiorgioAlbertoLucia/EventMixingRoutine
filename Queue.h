#pragma once

#include "TError.h"
#include <deque>

template <class T>
class Queue
{
    public:
        Queue() : m_collection(), m_depth(5){};
        Queue(unsigned int depth) : m_collection()
        {
          m_depth = depth;
        }

        void Fill(T &object)
        {
          if (IsFull())
          {
            m_collection.pop_front();
          }
          m_collection.push_back(object);
        }

        T &GetElement(int index)
        {
          if (index < 0 || index >= GetSize())
          {
            ::Fatal("Queue::GetElement", "index out of range");
            return m_collection[0];
          }
          else
          {
            return m_collection[index];
          }
        }

        int GetSize() { return (int)m_collection.size(); }

        void SetDepth(unsigned int depth)
        {
          m_depth = depth;
          m_collection.clear();
        }

        int GetDepth() { return (int)m_depth; }

        bool IsFull() { return m_collection.size() == m_depth; }

        bool IsEmpty() { return (int)m_collection.size() == 0; }

       private:
        std::deque<T> m_collection;
        unsigned int m_depth;
};
