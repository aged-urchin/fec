#ifndef ___REFCOUNT_H___
#define ___REFCOUNT_H___

#include <mutex>

class RefCount
{
public:
    virtual unsigned long retain()
    {
        m_lock.lock();
        const unsigned long refs = ++m_refs;
        m_lock.unlock();

        return refs;
    }

    virtual unsigned long release()
    {
        m_lock.lock();
        const unsigned long refs = --m_refs;
        m_lock.unlock();

        if (refs == 0)
        {
            delete this;
        }

        return refs;
    }

protected:
    RefCount()
    {
        m_refs = 1;
    }

    virtual ~RefCount() = default;

private:

    unsigned long   m_refs;
    std::mutex      m_lock;
};

#endif ///< ___REFCOUNT_H___
