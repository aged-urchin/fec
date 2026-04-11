#ifndef ___NUMBERUNWRAPPER_H___
#define ___NUMBERUNWRAPPER_H___

#include <cstdint>
#include <limits>

////////////////////////////////////////////////////////////////////////////////////////////////////
////
/** utility class to unwrap a number to a larger type. the numbers will never be unwrapped to a negative value.
 */
template <typename U>
class NumberUnwrapper
{
    static_assert(!std::numeric_limits<U>::is_signed, "class unwrapper only accepts unsigned types");
    static_assert((std::numeric_limits<U>::max)() <= (std::numeric_limits<uint32_t>::max)(), "class unwrapper only accepts types no wider than 32 bits");

public:
    /** get the unwrapped value, but don't update the internal state.
     */
    int64_t unwrap_without_update(U value)
    {
        if (!m_last_value_set)
        {
            return value;
        }

        constexpr int64_t kMaxPlusOne = static_cast<int64_t>((std::numeric_limits<U>::max)()) + 1;

        U cropped_last = static_cast<U>(m_last_value);
        int64_t delta = value - cropped_last;

        if (is_newer_value(value, cropped_last))
        {
            if (delta < 0)
            {
                /** wrap forwards.
                 */
                delta += kMaxPlusOne;
            }
        }
        else if (delta > 0 && (m_last_value + delta - kMaxPlusOne) >= 0)
        {
            /** if value is older but delta is positive, this is a backwards wrap-around. However, don't wrap backwards past 0 (unwrapped).
             */
            delta -= kMaxPlusOne;
        }

        return m_last_value + delta;
    }

    /** Only update the internal state to the specified last (unwrapped) value.
     */
    void update_last(int64_t last_value)
    {
        m_last_value = last_value;
        m_last_value_set = true;
    }

    /** unwrap the value and update the internal state.
     */
    int64_t unwrap(U value)
    {
        int64_t unwrapped = unwrap_without_update(value);
        update_last(unwrapped);

        return unwrapped;
    }

    /** reset
     */
    void reset()
    {
        m_last_value_set = false;
    }

    /** (C99): A computation involving unsigned operands can never overflow,
     *  because a result that cannot be represented by the resulting unsigned integer type is reduced
     *  modulo to the number that is one greater than the largest value that can be represented by the resulting type.
     */
    static bool
    is_newer_value(U value, U prev_value)
    {
        /** kBreakpoint is the half-way mark for the type U.
         *  for instance, for <uint16_t> it will be 0x8000, and for <uint32_t> it will be 0x8000000.
         *  after half of wrap-around period it is impossible to unwrap correctly.
         */
        constexpr U kBreakpoint = ((std::numeric_limits<U>::max)() >> 1) + 1;

        /** distinguish between elements that are exactly kBreakpoint apart.
         *  if t1 > t2 and |t1 - t2| = kBreakpoint: is_newer_value(t1, t2) = true, is_newer_value(t2, t1) = false
         *  rather than having is_newer_value(t1, t2) = is_newer_value(t2, t1) = false.
         */
        if (value - prev_value == kBreakpoint)
        {
            return value > prev_value;
        }

        return value != prev_value && static_cast<U>(value - prev_value) < kBreakpoint;
    }

private:

    int64_t     m_last_value;
    bool        m_last_value_set = false;
};

#endif ///< ___NUMBERUNWRAPPER_H___
