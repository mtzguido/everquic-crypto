

#include "EverQuic.h"

#include "internal/LowParse.h"
#include "internal/EverQuic_Krmllib.h"
#include "internal/EverQuic_EverCrypt.h"

static uint64_t min64(uint64_t x, uint64_t y)
{
  uint64_t cond = 1ULL & ~FStar_UInt64_gte_mask(x, y);
  return cond * x + (cond ^ (uint64_t)1U) * y;
}

static uint64_t max64(uint64_t x, uint64_t y)
{
  uint64_t cond = 1ULL & ~FStar_UInt64_gte_mask(y, x);
  return cond * x + (cond ^ (uint64_t)1U) * y;
}

static uint32_t secrets_are_equal_32_2(uint32_t x, uint32_t y)
{
  return 1U & FStar_UInt32_eq_mask(x, y);
}

static uint64_t secret_is_le_64(uint64_t x, uint64_t y)
{
  return (1ULL & ~FStar_UInt64_gte_mask(y, x)) ^ (uint64_t)1U;
}

static uint64_t secret_is_lt_64(uint64_t x, uint64_t y)
{
  return 1ULL & ~FStar_UInt64_gte_mask(x, y);
}

static uint64_t secrets_are_equal_64_2(uint64_t x, uint64_t y)
{
  return 1ULL & FStar_UInt64_eq_mask(x, y);
}

static uint64_t secrets_are_equal_62(uint64_t x, uint64_t y)
{
  return 1ULL & FStar_UInt64_eq_mask(x, y);
}

static uint64_t min640(uint64_t x, uint64_t y)
{
  return min64(x, y);
}

static uint64_t max640(uint64_t x, uint64_t y)
{
  return max64(x, y);
}

static uint32_t varint_len(uint64_t x)
{
  if (x < 64ULL)
    return 1U;
  else if (x < 16384ULL)
    return 2U;
  else if (x < 1073741824ULL)
    return 4U;
  else
    return 8U;
}

static uint32_t read_u32(uint8_t *b)
{
  uint8_t b3 = b[3U];
  uint8_t b2 = b[2U];
  uint8_t b1 = b[1U];
  uint8_t b0 = b[0U];
  return (uint32_t)b3 + 256U * ((uint32_t)b2 + 256U * ((uint32_t)b1 + 256U * (uint32_t)b0));
}

static uint64_t bound_npn(uint32_t pn_len)
{
  uint64_t pn_len_1 = (uint64_t)(pn_len - 1U);
  return
    secrets_are_equal_64_2(pn_len_1,
      (uint64_t)0U)
    * 256ULL
    + secrets_are_equal_64_2(pn_len_1, (uint64_t)1U) * 65536ULL
    + secrets_are_equal_64_2(pn_len_1, (uint64_t)2U) * 16777216ULL
    + secrets_are_equal_64_2(pn_len_1, (uint64_t)3U) * 4294967296ULL;
}

static uint64_t expand_pn(uint32_t pn_len, uint64_t last, uint32_t npn)
{
  uint64_t expected = last + 1ULL;
  uint64_t bound = bound_npn(pn_len);
  uint64_t bound_2 = bound >> 1U;
  uint64_t candidate = expected - (expected & (bound - 1ULL)) + (uint64_t)npn;
  uint64_t bound_2_le_expected = secret_is_le_64(bound_2, expected);
  uint64_t
  cond_1 =
    bound_2_le_expected
    * secret_is_le_64(candidate, bound_2_le_expected * expected - bound_2_le_expected * bound_2)
    * secret_is_lt_64(candidate, 4611686018427387904ULL - bound);
  uint64_t
  cond_2 =
    (cond_1 ^ (uint64_t)1U)
    * secret_is_lt_64(expected + bound_2, candidate)
    * secret_is_le_64(bound, candidate);
  return candidate + cond_1 * bound - cond_2 * bound;
}

static uint64_t read_packet_number(uint64_t last, uint32_t pn_len, uint8_t *b)
{
  uint32_t x = read_u32(b);
  uint32_t pn_len_1 = pn_len - 1U;
  uint32_t op10 = x << 0U;
  uint32_t op11 = x << 0U;
  uint32_t op1 = x << 0U;
  uint32_t
  npn =
    secrets_are_equal_32_2(pn_len_1,
      0U)
    * (op10 >> 24U)
    + secrets_are_equal_32_2(pn_len_1, 1U) * (op11 >> 16U)
    + secrets_are_equal_32_2(pn_len_1, 2U) * (op1 >> 8U)
    + secrets_are_equal_32_2(pn_len_1, 3U) * x;
  return expand_pn(pn_len, last, npn);
}

static uint32_t reduce_pn(uint32_t pn_len, uint64_t pn)
{
  uint64_t mask = bound_npn(pn_len) - 1ULL;
  return (uint32_t)(pn & mask);
}

static void write_u32(uint32_t x, uint8_t *b)
{
  uint8_t *b_ = b;
  b_[0U] = (uint8_t)(x >> 24U);
  b_[1U] = (uint8_t)(x >> 16U);
  b_[2U] = (uint8_t)(x >> 8U);
  b_[3U] = (uint8_t)x;
}

static uint32_t set_left_bitfield(uint32_t pn_len, uint32_t before, uint32_t x)
{
  uint32_t op00 = 0xFFFFFFFFU;
  uint32_t op1 = op00 >> 24U;
  uint32_t op2 = op1 << 24U;
  uint32_t op3 = op2 ^ 0xFFFFFFFFU;
  uint32_t op4 = before & op3;
  uint32_t op50 = (x & 255U) << 24U;
  uint32_t op01 = 0xFFFFFFFFU;
  uint32_t op10 = op01 >> 16U;
  uint32_t op20 = op10 << 16U;
  uint32_t op30 = op20 ^ 0xFFFFFFFFU;
  uint32_t op40 = before & op30;
  uint32_t op51 = (x & 65535U) << 16U;
  uint32_t op0 = 0xFFFFFFFFU;
  uint32_t op11 = op0 >> 8U;
  uint32_t op21 = op11 << 8U;
  uint32_t op31 = op21 ^ 0xFFFFFFFFU;
  uint32_t op41 = before & op31;
  uint32_t op5 = (x & 16777215U) << 8U;
  return
    secrets_are_equal_32_2(pn_len - 1U,
      0U)
    * (op4 | op50)
    + secrets_are_equal_32_2(pn_len - 1U, 1U) * (op40 | op51)
    + secrets_are_equal_32_2(pn_len - 1U, 2U) * (op41 | op5)
    + secrets_are_equal_32_2(pn_len - 1U, 3U) * x;
}

static void write_bounded_integer(uint32_t pn_len, uint32_t x, uint8_t *b)
{
  uint8_t *b_ = b;
  uint32_t before = read_u32(b_);
  uint32_t after = set_left_bitfield(pn_len, before, x);
  write_u32(after, b_);
}

static void write_packet_number(uint32_t pn_len, uint64_t pn, uint8_t *b)
{
  uint32_t npn = reduce_pn(pn_len, pn);
  write_bounded_integer(pn_len, npn, b);
}

static uint64_t validate_varint(LowParse_Slice_slice sl, uint64_t pos)
{
  uint64_t pos0 = pos;
  uint32_t pos1 = (uint32_t)pos;
  uint64_t pos11;
  if ((uint64_t)sl.len - pos0 < 1ULL)
    pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
  else
    pos11 = pos0 + 1ULL;
  if (LowParse_Low_ErrorCode_is_error(pos11))
    return pos11;
  else
  {
    uint8_t b = sl.base[pos1];
    uint8_t kd = LowParse_BitFields_get_bitfield_gen8(b, 6U, 8U);
    uint8_t msb8 = LowParse_BitFields_get_bitfield_gen8(b, 0U, 6U);
    uint64_t msb = (uint64_t)msb8;
    if (kd == 0U)
      return pos11;
    else
    {
      uint64_t pos1_ = pos11;
      uint32_t pos12 = (uint32_t)pos11;
      if (kd == 1U)
      {
        uint64_t pos2;
        if ((uint64_t)sl.len - pos1_ < 1ULL)
          pos2 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
        else
          pos2 = pos1_ + 1ULL;
        if (LowParse_Low_ErrorCode_is_error(pos2))
          return pos2;
        else
        {
          uint8_t lsb = sl.base[pos12];
          uint64_t z = msb * 256ULL + (uint64_t)lsb;
          if (64ULL <= z)
            return pos2;
          else
            return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
        }
      }
      else if (kd == 2U)
      {
        uint64_t pos2;
        if ((uint64_t)sl.len - pos1_ < 3ULL)
          pos2 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
        else
          pos2 = pos1_ + 3ULL;
        if (LowParse_Low_ErrorCode_is_error(pos2))
          return pos2;
        else
        {
          uint8_t lo = sl.base[pos12 + 2U];
          uint16_t hi = load16_be(sl.base + pos12);
          uint32_t lsb = (uint32_t)lo + ((uint32_t)hi << 8U);
          uint64_t z = msb * 16777216ULL + (uint64_t)lsb;
          if (16384ULL <= z)
            return pos2;
          else
            return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
        }
      }
      else
      {
        uint64_t pos13;
        if ((uint64_t)sl.len - pos1_ < 4ULL)
          pos13 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
        else
          pos13 = pos1_ + 4ULL;
        uint64_t pos2;
        if (LowParse_Low_ErrorCode_is_error(pos13))
          pos2 = pos13;
        else if ((uint64_t)sl.len - pos13 < 3ULL)
          pos2 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
        else
          pos2 = pos13 + 3ULL;
        if (LowParse_Low_ErrorCode_is_error(pos2))
          return pos2;
        else
        {
          uint32_t pos_hi = pos12;
          uint32_t hi = load32_be(sl.base + pos_hi);
          uint32_t pos_lo = pos12 + 4U;
          uint8_t lo = sl.base[pos_lo + 2U];
          uint16_t hi1 = load16_be(sl.base + pos_lo);
          uint32_t lo0 = (uint32_t)lo + ((uint32_t)hi1 << 8U);
          uint64_t z = (uint64_t)lo0 + 16777216ULL * ((uint64_t)hi + 4294967296ULL * msb);
          if (1073741824ULL <= z)
            return pos2;
          else
            return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
        }
      }
    }
  }
}

static uint64_t read_varint(LowParse_Slice_slice sl, uint32_t pos)
{
  uint32_t pos1 = pos + 1U;
  uint8_t b = sl.base[pos];
  uint8_t kd = LowParse_BitFields_get_bitfield_gen8(b, 6U, 8U);
  uint8_t msb8 = LowParse_BitFields_get_bitfield_gen8(b, 0U, 6U);
  uint64_t msb = (uint64_t)msb8;
  if (kd == 0U)
    return msb;
  else if (kd == 1U)
  {
    uint8_t lsb = sl.base[pos1];
    return msb * 256ULL + (uint64_t)lsb;
  }
  else if (kd == 2U)
  {
    uint8_t lo = sl.base[pos1 + 2U];
    uint16_t hi = load16_be(sl.base + pos1);
    uint32_t lsb = (uint32_t)lo + ((uint32_t)hi << 8U);
    return msb * 16777216ULL + (uint64_t)lsb;
  }
  else
  {
    uint32_t pos_hi = pos1;
    uint32_t hi = load32_be(sl.base + pos_hi);
    uint32_t pos_lo = pos1 + 4U;
    uint8_t lo = sl.base[pos_lo + 2U];
    uint16_t hi1 = load16_be(sl.base + pos_lo);
    uint32_t lo0 = (uint32_t)lo + ((uint32_t)hi1 << 8U);
    return (uint64_t)lo0 + 16777216ULL * ((uint64_t)hi + 4294967296ULL * msb);
  }
}

static uint32_t jump_varint(LowParse_Slice_slice sl, uint32_t pos)
{
  uint32_t pos1 = pos + 1U;
  uint8_t b = sl.base[pos];
  uint8_t kd = LowParse_BitFields_get_bitfield_gen8(b, 6U, 8U);
  LowParse_BitFields_get_bitfield_gen8(b, 0U, 6U);
  if (kd == 0U)
    return pos1;
  else if (kd == 1U)
    return pos1 + 1U;
  else if (kd == 2U)
    return pos1 + 3U;
  else
    return pos1 + 4U + 3U;
}

static uint32_t write_varint(uint64_t x, uint8_t *b, uint32_t pos)
{
  uint64_t ite0;
  if (x < 64ULL)
    ite0 = x;
  else if (x < 16384ULL)
    ite0 = x / 256ULL;
  else if (x < 1073741824ULL)
    ite0 = x / 16777216ULL;
  else
    ite0 = x / 72057594037927936ULL;
  uint8_t ite;
  if (x < 64ULL)
    ite = 0U;
  else if (x < 16384ULL)
    ite = 1U;
  else if (x < 1073741824ULL)
    ite = 2U;
  else
    ite = 3U;
  uint8_t
  fb =
    LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(0U,
        0U,
        6U,
        (uint8_t)ite0),
      6U,
      8U,
      ite);
  b[pos] = fb;
  uint32_t res0 = 1U;
  uint32_t len1 = res0;
  uint32_t pos1 = pos + len1;
  if (x < 64ULL)
    return len1;
  else
  {
    uint32_t len2;
    if (x < 16384ULL)
    {
      b[pos1] = (uint8_t)x;
      uint32_t res = 1U;
      len2 = res;
    }
    else if (x < 1073741824ULL)
    {
      uint8_t lo = (uint8_t)(uint32_t)(x % 16777216ULL);
      b[pos1 + 2U] = lo;
      uint32_t hi_ = (uint32_t)(x % 16777216ULL) >> 8U;
      uint16_t hi = (uint16_t)hi_;
      store16_be(b + pos1, hi);
      uint32_t res = 3U;
      len2 = res;
    }
    else
    {
      uint32_t x2 = (uint32_t)(x / 16777216ULL);
      store32_be(b + pos1, x2);
      uint32_t res = 4U;
      uint32_t len11 = res;
      uint32_t pos11 = pos1 + len11;
      uint8_t lo = (uint8_t)(uint32_t)(x % 16777216ULL);
      b[pos11 + 2U] = lo;
      uint32_t hi_ = (uint32_t)(x % 16777216ULL) >> 8U;
      uint16_t hi = (uint16_t)hi_;
      store16_be(b + pos11, hi);
      uint32_t res0 = 3U;
      uint32_t len20 = res0;
      uint32_t res1 = len11 + len20;
      len2 = res1;
    }
    return len1 + len2;
  }
}

static uint64_t
validate_bounded_varint(uint32_t min, uint32_t max, LowParse_Slice_slice input, uint64_t pos)
{
  uint64_t res = validate_varint(input, pos);
  if (LowParse_Low_ErrorCode_is_error(res))
    return res;
  else
  {
    uint64_t va = read_varint(input, (uint32_t)pos);
    if (!((uint64_t)min <= va && va <= (uint64_t)max))
      return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
    else
      return res;
  }
}

static uint64_t validate_header(uint32_t short_dcid_len, LowParse_Slice_slice sl, uint64_t pos)
{
  uint64_t res0;
  if ((uint64_t)sl.len - pos < 1ULL)
    res0 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
  else
    res0 = pos + 1ULL;
  uint64_t pos1;
  if (LowParse_Low_ErrorCode_is_error(res0))
    pos1 = res0;
  else
  {
    uint8_t va = sl.base[(uint32_t)pos];
    uint8_t xr = (uint32_t)va & 128U;
    bool ite;
    if (xr == 128U)
    {
      uint8_t xr1 = (uint32_t)va & 64U;
      if (xr1 == 64U)
      {
        uint8_t xr2 = (uint32_t)va & 48U;
        if (xr2 == 0U)
          ite = true;
        else if (xr2 == 16U)
          ite = true;
        else if (xr2 == 32U)
          ite = true;
        else if (xr2 == 48U)
          ite = true;
        else
          ite = false;
      }
      else
        ite = false;
    }
    else if (xr == 0U)
    {
      uint8_t xr1 = (uint32_t)va & 64U;
      if (xr1 == 64U)
        ite = true;
      else
        ite = false;
    }
    else
      ite = false;
    if (!ite)
      pos1 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
    else
      pos1 = res0;
  }
  if (LowParse_Low_ErrorCode_is_error(pos1))
    return pos1;
  else
  {
    uint8_t x = sl.base[(uint32_t)pos];
    uint8_t xr = (uint32_t)x & 128U;
    if (xr == 128U)
    {
      uint8_t xr1 = (uint32_t)x & 64U;
      if (xr1 == 64U)
      {
        uint8_t xr2 = (uint32_t)x & 48U;
        if (xr2 == 0U)
        {
          uint64_t pos110;
          if ((uint64_t)sl.len - pos1 < 4ULL)
            pos110 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
          else
            pos110 = pos1 + 4ULL;
          uint64_t pos11;
          if (LowParse_Low_ErrorCode_is_error(pos110))
            pos11 = pos110;
          else
          {
            uint64_t res0;
            if ((uint64_t)sl.len - pos110 < 1ULL)
              res0 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
            else
              res0 = pos110 + 1ULL;
            uint64_t pos12;
            if (LowParse_Low_ErrorCode_is_error(res0))
              pos12 = res0;
            else
            {
              uint8_t r = sl.base[(uint32_t)pos110];
              uint32_t va = (uint32_t)r;
              if (va <= 20U)
                if ((uint64_t)sl.len - res0 < (uint64_t)va)
                  pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                else
                {
                  uint64_t pos_ = (uint64_t)((uint32_t)res0 + va);
                  if (LowParse_Low_ErrorCode_is_error(pos_))
                    if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                      pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                    else
                      pos12 = pos_;
                  else
                    pos12 = pos_;
                }
              else
                pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
            }
            if (LowParse_Low_ErrorCode_is_error(pos12))
              pos11 = pos12;
            else
            {
              uint64_t res;
              if ((uint64_t)sl.len - pos12 < 1ULL)
                res = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
              else
                res = pos12 + 1ULL;
              if (LowParse_Low_ErrorCode_is_error(res))
                pos11 = res;
              else
              {
                uint8_t r = sl.base[(uint32_t)pos12];
                uint32_t va = (uint32_t)r;
                if (va <= 20U)
                  if ((uint64_t)sl.len - res < (uint64_t)va)
                    pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                  else
                  {
                    uint64_t pos_ = (uint64_t)((uint32_t)res + va);
                    if (LowParse_Low_ErrorCode_is_error(pos_))
                      if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                        pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                      else
                        pos11 = pos_;
                    else
                      pos11 = pos_;
                  }
                else
                  pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
              }
            }
          }
          if (LowParse_Low_ErrorCode_is_error(pos11))
            return pos11;
          else
          {
            uint64_t n = validate_bounded_varint(0U, 16383U, sl, pos11);
            uint64_t pos12;
            if (LowParse_Low_ErrorCode_is_error(n))
              pos12 = n;
            else
            {
              uint64_t res = read_varint(sl, (uint32_t)pos11);
              uint32_t len = (uint32_t)res;
              if ((uint64_t)sl.len - n < (uint64_t)len)
                pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
              else
              {
                uint64_t pos_ = (uint64_t)((uint32_t)n + len);
                if (LowParse_Low_ErrorCode_is_error(pos_))
                  if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                    pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                  else
                    pos12 = pos_;
                else
                  pos12 = pos_;
              }
            }
            if (LowParse_Low_ErrorCode_is_error(pos12))
              return pos12;
            else
            {
              uint64_t res = validate_varint(sl, pos12);
              if (LowParse_Low_ErrorCode_is_error(res))
                return res;
              else
              {
                uint64_t va = read_varint(sl, (uint32_t)pos12);
                if (!(va >= 20ULL))
                  return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                else
                  return res;
              }
            }
          }
        }
        else if (xr2 == 16U)
        {
          uint64_t pos110;
          if ((uint64_t)sl.len - pos1 < 4ULL)
            pos110 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
          else
            pos110 = pos1 + 4ULL;
          uint64_t pos11;
          if (LowParse_Low_ErrorCode_is_error(pos110))
            pos11 = pos110;
          else
          {
            uint64_t res0;
            if ((uint64_t)sl.len - pos110 < 1ULL)
              res0 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
            else
              res0 = pos110 + 1ULL;
            uint64_t pos12;
            if (LowParse_Low_ErrorCode_is_error(res0))
              pos12 = res0;
            else
            {
              uint8_t r = sl.base[(uint32_t)pos110];
              uint32_t va = (uint32_t)r;
              if (va <= 20U)
                if ((uint64_t)sl.len - res0 < (uint64_t)va)
                  pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                else
                {
                  uint64_t pos_ = (uint64_t)((uint32_t)res0 + va);
                  if (LowParse_Low_ErrorCode_is_error(pos_))
                    if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                      pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                    else
                      pos12 = pos_;
                  else
                    pos12 = pos_;
                }
              else
                pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
            }
            if (LowParse_Low_ErrorCode_is_error(pos12))
              pos11 = pos12;
            else
            {
              uint64_t res;
              if ((uint64_t)sl.len - pos12 < 1ULL)
                res = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
              else
                res = pos12 + 1ULL;
              if (LowParse_Low_ErrorCode_is_error(res))
                pos11 = res;
              else
              {
                uint8_t r = sl.base[(uint32_t)pos12];
                uint32_t va = (uint32_t)r;
                if (va <= 20U)
                  if ((uint64_t)sl.len - res < (uint64_t)va)
                    pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                  else
                  {
                    uint64_t pos_ = (uint64_t)((uint32_t)res + va);
                    if (LowParse_Low_ErrorCode_is_error(pos_))
                      if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                        pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                      else
                        pos11 = pos_;
                    else
                      pos11 = pos_;
                  }
                else
                  pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
              }
            }
          }
          if (LowParse_Low_ErrorCode_is_error(pos11))
            return pos11;
          else
          {
            uint64_t res = validate_varint(sl, pos11);
            if (LowParse_Low_ErrorCode_is_error(res))
              return res;
            else
            {
              uint64_t va = read_varint(sl, (uint32_t)pos11);
              if (!(va >= 20ULL))
                return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
              else
                return res;
            }
          }
        }
        else if (xr2 == 32U)
        {
          uint64_t pos110;
          if ((uint64_t)sl.len - pos1 < 4ULL)
            pos110 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
          else
            pos110 = pos1 + 4ULL;
          uint64_t pos11;
          if (LowParse_Low_ErrorCode_is_error(pos110))
            pos11 = pos110;
          else
          {
            uint64_t res0;
            if ((uint64_t)sl.len - pos110 < 1ULL)
              res0 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
            else
              res0 = pos110 + 1ULL;
            uint64_t pos12;
            if (LowParse_Low_ErrorCode_is_error(res0))
              pos12 = res0;
            else
            {
              uint8_t r = sl.base[(uint32_t)pos110];
              uint32_t va = (uint32_t)r;
              if (va <= 20U)
                if ((uint64_t)sl.len - res0 < (uint64_t)va)
                  pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                else
                {
                  uint64_t pos_ = (uint64_t)((uint32_t)res0 + va);
                  if (LowParse_Low_ErrorCode_is_error(pos_))
                    if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                      pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                    else
                      pos12 = pos_;
                  else
                    pos12 = pos_;
                }
              else
                pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
            }
            if (LowParse_Low_ErrorCode_is_error(pos12))
              pos11 = pos12;
            else
            {
              uint64_t res;
              if ((uint64_t)sl.len - pos12 < 1ULL)
                res = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
              else
                res = pos12 + 1ULL;
              if (LowParse_Low_ErrorCode_is_error(res))
                pos11 = res;
              else
              {
                uint8_t r = sl.base[(uint32_t)pos12];
                uint32_t va = (uint32_t)r;
                if (va <= 20U)
                  if ((uint64_t)sl.len - res < (uint64_t)va)
                    pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                  else
                  {
                    uint64_t pos_ = (uint64_t)((uint32_t)res + va);
                    if (LowParse_Low_ErrorCode_is_error(pos_))
                      if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                        pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                      else
                        pos11 = pos_;
                    else
                      pos11 = pos_;
                  }
                else
                  pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
              }
            }
          }
          if (LowParse_Low_ErrorCode_is_error(pos11))
            return pos11;
          else
          {
            uint64_t res = validate_varint(sl, pos11);
            if (LowParse_Low_ErrorCode_is_error(res))
              return res;
            else
            {
              uint64_t va = read_varint(sl, (uint32_t)pos11);
              if (!(va >= 20ULL))
                return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
              else
                return res;
            }
          }
        }
        else if (xr2 == 48U)
        {
          uint64_t pos110;
          if ((uint64_t)sl.len - pos1 < 4ULL)
            pos110 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
          else
            pos110 = pos1 + 4ULL;
          uint64_t pos11;
          if (LowParse_Low_ErrorCode_is_error(pos110))
            pos11 = pos110;
          else
          {
            uint64_t res0;
            if ((uint64_t)sl.len - pos110 < 1ULL)
              res0 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
            else
              res0 = pos110 + 1ULL;
            uint64_t pos12;
            if (LowParse_Low_ErrorCode_is_error(res0))
              pos12 = res0;
            else
            {
              uint8_t r = sl.base[(uint32_t)pos110];
              uint32_t va = (uint32_t)r;
              if (va <= 20U)
                if ((uint64_t)sl.len - res0 < (uint64_t)va)
                  pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                else
                {
                  uint64_t pos_ = (uint64_t)((uint32_t)res0 + va);
                  if (LowParse_Low_ErrorCode_is_error(pos_))
                    if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                      pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                    else
                      pos12 = pos_;
                  else
                    pos12 = pos_;
                }
              else
                pos12 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
            }
            if (LowParse_Low_ErrorCode_is_error(pos12))
              pos11 = pos12;
            else
            {
              uint64_t res;
              if ((uint64_t)sl.len - pos12 < 1ULL)
                res = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
              else
                res = pos12 + 1ULL;
              if (LowParse_Low_ErrorCode_is_error(res))
                pos11 = res;
              else
              {
                uint8_t r = sl.base[(uint32_t)pos12];
                uint32_t va = (uint32_t)r;
                if (va <= 20U)
                  if ((uint64_t)sl.len - res < (uint64_t)va)
                    pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                  else
                  {
                    uint64_t pos_ = (uint64_t)((uint32_t)res + va);
                    if (LowParse_Low_ErrorCode_is_error(pos_))
                      if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                        pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                      else
                        pos11 = pos_;
                    else
                      pos11 = pos_;
                  }
                else
                  pos11 = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
              }
            }
          }
          if (LowParse_Low_ErrorCode_is_error(pos11))
            return pos11;
          else
          {
            uint64_t res;
            if ((uint64_t)sl.len - pos11 < 1ULL)
              res = LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
            else
              res = pos11 + 1ULL;
            if (LowParse_Low_ErrorCode_is_error(res))
              return res;
            else
            {
              uint8_t r = sl.base[(uint32_t)pos11];
              uint32_t va = (uint32_t)r;
              if (va <= 20U)
                if ((uint64_t)sl.len - res < (uint64_t)va)
                  return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
                else
                {
                  uint64_t pos_ = (uint64_t)((uint32_t)res + va);
                  if (LowParse_Low_ErrorCode_is_error(pos_))
                    if (pos_ == LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA)
                      return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
                    else
                      return pos_;
                  else
                    return pos_;
                }
              else
                return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
            }
          }
        }
        else
          return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
      }
      else
        return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
    }
    else if (xr == 0U)
    {
      uint8_t xr1 = (uint32_t)x & 64U;
      if (xr1 == 64U)
        if ((uint64_t)sl.len - pos1 < (uint64_t)short_dcid_len)
          return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_NOT_ENOUGH_DATA;
        else
          return pos1 + (uint64_t)short_dcid_len;
      else
        return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
    }
    else
      return LOWPARSE_LOW_ERRORCODE_VALIDATOR_ERROR_GENERIC;
  }
}

#define PInitial 0
#define PZeroRTT 1
#define PHandshake 2
#define PRetry 3

typedef uint8_t long_header_specifics_tags;

typedef struct long_header_specifics_s
{
  long_header_specifics_tags tag;
  union {
    struct
    {
      uint64_t payload_and_pn_length;
      uint8_t *token;
      uint32_t token_length;
    }
    case_PInitial;
    uint64_t case_PZeroRTT;
    uint64_t case_PHandshake;
    struct
    {
      uint8_t *odcid;
      uint32_t odcil;
    }
    case_PRetry;
  }
  ;
}
long_header_specifics;

static bool uu___is_PRetry(long_header_specifics projectee)
{
  if (projectee.tag == PRetry)
    return true;
  else
    return false;
}

#define PLong 0
#define PShort 1

typedef uint8_t header_tags;

typedef struct header_s
{
  header_tags tag;
  union {
    struct
    {
      uint8_t protected_bits;
      uint32_t version;
      uint8_t *dcid;
      uint32_t dcil;
      uint8_t *scid;
      uint32_t scil;
      long_header_specifics spec;
    }
    case_PLong;
    struct
    {
      uint8_t protected_bits;
      bool spin;
      uint8_t *cid;
      uint32_t cid_len;
    }
    case_PShort;
  }
  ;
}
header;

static bool uu___is_PShort(header projectee)
{
  if (projectee.tag == PShort)
    return true;
  else
    return false;
}

static bool is_retry(header h)
{
  if (uu___is_PShort(h))
    return false;
  else
  {
    long_header_specifics spec;
    if (h.tag == PLong)
      spec = h.case_PLong.spec;
    else
      spec =
        KRML_EABORT(long_header_specifics,
          "unreachable (pattern matches are exhaustive in F*)");
    return uu___is_PRetry(spec);
  }
}

static header
read_header_body_short(
  LowParse_Slice_slice sl,
  uint32_t cid_len,
  uint8_t spin,
  uint8_t protected_bits
)
{
  uint8_t *dcid = sl.base + 1U;
  return
    (
      (header){
        .tag = PShort,
        {
          .case_PShort = {
            .protected_bits = protected_bits,
            .spin = spin == 1U,
            .cid = dcid,
            .cid_len = cid_len
          }
        }
      }
    );
}

static header read_header_body_long_retry(LowParse_Slice_slice sl, uint8_t protected_bits)
{
  uint32_t version = load32_be(sl.base + 1U);
  uint32_t pos1 = 5U;
  uint8_t *dcid = sl.base + pos1 + 1U;
  uint8_t r0 = sl.base[pos1];
  uint32_t dcid_len1 = (uint32_t)r0;
  uint8_t r1 = sl.base[pos1];
  uint32_t pos2 = pos1 + 1U + (uint32_t)r1;
  uint8_t *scid = sl.base + pos2 + 1U;
  uint8_t r2 = sl.base[pos2];
  uint32_t scid_len = (uint32_t)r2;
  uint8_t r3 = sl.base[pos2];
  uint32_t pos3 = pos2 + 1U + (uint32_t)r3;
  uint8_t *odcid = sl.base + pos3 + 1U;
  uint8_t r = sl.base[pos3];
  uint32_t odcid_len = (uint32_t)r;
  long_header_specifics
  spec = { .tag = PRetry, { .case_PRetry = { .odcid = odcid, .odcil = odcid_len } } };
  return
    (
      (header){
        .tag = PLong,
        {
          .case_PLong = {
            .protected_bits = protected_bits, .version = version, .dcid = dcid, .dcil = dcid_len1,
            .scid = scid, .scil = scid_len, .spec = spec
          }
        }
      }
    );
}

static header read_header_body_long_initial(LowParse_Slice_slice sl, uint8_t protected_bits)
{
  uint32_t version = load32_be(sl.base + 1U);
  uint32_t pos1 = 5U;
  uint8_t *dcid = sl.base + pos1 + 1U;
  uint8_t r0 = sl.base[pos1];
  uint32_t dcid_len1 = (uint32_t)r0;
  uint8_t r1 = sl.base[pos1];
  uint32_t pos2 = pos1 + 1U + (uint32_t)r1;
  uint8_t *scid = sl.base + pos2 + 1U;
  uint8_t r2 = sl.base[pos2];
  uint32_t scid_len = (uint32_t)r2;
  uint8_t r = sl.base[pos2];
  uint32_t pos3 = pos2 + 1U + (uint32_t)r;
  uint64_t res0 = read_varint(sl, pos3);
  uint32_t len1 = (uint32_t)res0;
  KRML_MAYBE_UNUSED_VAR(len1);
  uint32_t pos11 = jump_varint(sl, pos3);
  uint8_t *token = sl.base + pos11;
  uint64_t res1 = read_varint(sl, pos3);
  uint32_t len10 = (uint32_t)res1;
  uint32_t token_len = len10;
  uint32_t n = jump_varint(sl, pos3);
  uint64_t res = read_varint(sl, pos3);
  uint32_t len11 = (uint32_t)res;
  uint32_t pos4 = n + len11;
  uint64_t payload_and_pn_length = read_varint(sl, pos4);
  long_header_specifics
  spec =
    {
      .tag = PInitial,
      {
        .case_PInitial = {
          .payload_and_pn_length = payload_and_pn_length,
          .token = token,
          .token_length = token_len
        }
      }
    };
  return
    (
      (header){
        .tag = PLong,
        {
          .case_PLong = {
            .protected_bits = protected_bits, .version = version, .dcid = dcid, .dcil = dcid_len1,
            .scid = scid, .scil = scid_len, .spec = spec
          }
        }
      }
    );
}

static header read_header_body_long_handshake(LowParse_Slice_slice sl, uint8_t protected_bits)
{
  uint32_t version = load32_be(sl.base + 1U);
  uint32_t pos1 = 5U;
  uint8_t *dcid = sl.base + pos1 + 1U;
  uint8_t r0 = sl.base[pos1];
  uint32_t dcid_len1 = (uint32_t)r0;
  uint8_t r1 = sl.base[pos1];
  uint32_t pos2 = pos1 + 1U + (uint32_t)r1;
  uint8_t *scid = sl.base + pos2 + 1U;
  uint8_t r2 = sl.base[pos2];
  uint32_t scid_len = (uint32_t)r2;
  uint8_t r = sl.base[pos2];
  uint32_t pos3 = pos2 + 1U + (uint32_t)r;
  uint64_t payload_and_pn_length = read_varint(sl, pos3);
  long_header_specifics
  spec = { .tag = PHandshake, { .case_PHandshake = payload_and_pn_length } };
  return
    (
      (header){
        .tag = PLong,
        {
          .case_PLong = {
            .protected_bits = protected_bits, .version = version, .dcid = dcid, .dcil = dcid_len1,
            .scid = scid, .scil = scid_len, .spec = spec
          }
        }
      }
    );
}

static header read_header_body_long_zero_rtt(LowParse_Slice_slice sl, uint8_t protected_bits)
{
  uint32_t version = load32_be(sl.base + 1U);
  uint32_t pos1 = 5U;
  uint8_t *dcid = sl.base + pos1 + 1U;
  uint8_t r0 = sl.base[pos1];
  uint32_t dcid_len1 = (uint32_t)r0;
  uint8_t r1 = sl.base[pos1];
  uint32_t pos2 = pos1 + 1U + (uint32_t)r1;
  uint8_t *scid = sl.base + pos2 + 1U;
  uint8_t r2 = sl.base[pos2];
  uint32_t scid_len = (uint32_t)r2;
  uint8_t r = sl.base[pos2];
  uint32_t pos3 = pos2 + 1U + (uint32_t)r;
  uint64_t payload_and_pn_length = read_varint(sl, pos3);
  long_header_specifics spec = { .tag = PZeroRTT, { .case_PZeroRTT = payload_and_pn_length } };
  return
    (
      (header){
        .tag = PLong,
        {
          .case_PLong = {
            .protected_bits = protected_bits, .version = version, .dcid = dcid, .dcil = dcid_len1,
            .scid = scid, .scil = scid_len, .spec = spec
          }
        }
      }
    );
}

static header read_header(uint8_t *packet, uint32_t packet_len, uint32_t cid_len)
{
  LowParse_Slice_slice sl = { .base = packet, .len = packet_len };
  uint8_t r = sl.base[0U];
  if (LowParse_BitFields_get_bitfield_gen8(r, 7U, 8U) == 1U)
    if (LowParse_BitFields_get_bitfield_gen8(r, 4U, 6U) == 0U)
      return read_header_body_long_initial(sl, LowParse_BitFields_get_bitfield_gen8(r, 0U, 4U));
    else if (LowParse_BitFields_get_bitfield_gen8(r, 4U, 6U) == 1U)
      return read_header_body_long_zero_rtt(sl, LowParse_BitFields_get_bitfield_gen8(r, 0U, 4U));
    else if (LowParse_BitFields_get_bitfield_gen8(r, 4U, 6U) == 2U)
      return read_header_body_long_handshake(sl, LowParse_BitFields_get_bitfield_gen8(r, 0U, 4U));
    else
      return read_header_body_long_retry(sl, LowParse_BitFields_get_bitfield_gen8(r, 0U, 4U));
  else
    return
      read_header_body_short(sl,
        cid_len,
        LowParse_BitFields_get_bitfield_gen8(r, 5U, 6U),
        LowParse_BitFields_get_bitfield_gen8(r, 0U, 5U));
}

static uint8_t get_pb(header h)
{
  if (h.tag == PShort)
    return h.case_PShort.protected_bits;
  else if (h.tag == PLong)
    return h.case_PLong.protected_bits;
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static uint32_t write_header(header h, uint8_t *out, uint32_t out_len)
{
  uint8_t pb = get_pb(h);
  LowParse_Slice_slice sl = { .base = out, .len = out_len };
  uint32_t len0;
  if (h.tag == PShort)
  {
    uint32_t cid_len = h.case_PShort.cid_len;
    uint8_t *cid = h.case_PShort.cid;
    bool spin = h.case_PShort.spin;
    uint8_t ite;
    if (spin)
      ite = 1U;
    else
      ite = 0U;
    sl.base[0U] =
      LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(0U,
              0U,
              5U,
              0U),
            5U,
            6U,
            ite),
          6U,
          7U,
          1U),
        7U,
        8U,
        0U);
    uint32_t len = 1U;
    uint32_t pos1 = 0U + len;
    uint8_t *payload = sl.base + pos1;
    memcpy(payload, cid, cid_len * sizeof (uint8_t));
    uint32_t res = pos1 + cid_len;
    uint32_t pos2 = res;
    len0 = pos2;
  }
  else if (h.tag == PLong)
  {
    long_header_specifics spec = h.case_PLong.spec;
    uint32_t scil = h.case_PLong.scil;
    uint8_t *scid = h.case_PLong.scid;
    uint32_t dcil = h.case_PLong.dcil;
    uint8_t *dcid = h.case_PLong.dcid;
    uint32_t version = h.case_PLong.version;
    if (spec.tag == PInitial)
    {
      uint32_t token_length = spec.case_PInitial.token_length;
      uint8_t *token = spec.case_PInitial.token;
      uint64_t payload_and_pn_length = spec.case_PInitial.payload_and_pn_length;
      sl.base[0U] =
        LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(0U,
                0U,
                4U,
                0U),
              4U,
              6U,
              0U),
            6U,
            7U,
            1U),
          7U,
          8U,
          1U);
      uint32_t len1 = 1U;
      uint32_t pos1 = 0U + len1;
      store32_be(sl.base + pos1, version);
      uint32_t len = 4U;
      uint32_t pos11 = pos1 + len;
      uint32_t pout_payload0 = pos11 + 1U;
      uint8_t *payload0 = sl.base + pout_payload0;
      memcpy(payload0, dcid, dcil * sizeof (uint8_t));
      sl.base[pos11] = (uint8_t)dcil;
      uint32_t len2 = 1U;
      uint32_t pos_payload = pos11 + len2;
      uint32_t pos12 = pos_payload + dcil;
      uint32_t pout_payload = pos12 + 1U;
      uint8_t *payload1 = sl.base + pout_payload;
      memcpy(payload1, scid, scil * sizeof (uint8_t));
      sl.base[pos12] = (uint8_t)scil;
      uint32_t len3 = 1U;
      uint32_t pos_payload0 = pos12 + len3;
      uint32_t pos2 = pos_payload0 + scil;
      uint32_t pos20 = pos2;
      uint32_t pos110 = pos20;
      uint8_t *base0 = sl.base;
      uint32_t len4 = write_varint((uint64_t)token_length, (uint8_t *)base0, pos110);
      uint32_t pout_payload1 = pos110 + len4;
      uint8_t *payload = sl.base + pout_payload1;
      memcpy(payload, token, token_length * sizeof (uint8_t));
      uint32_t pos120 = pout_payload1 + token_length;
      uint8_t *base = sl.base;
      uint32_t len5 = write_varint(payload_and_pn_length, (uint8_t *)base, pos120);
      uint32_t res = pos120 + len5;
      uint32_t pos21 = res;
      uint32_t pos22 = pos21;
      uint32_t pos23 = pos22;
      len0 = pos23;
    }
    else if (spec.tag == PZeroRTT)
    {
      uint64_t payload_and_pn_length = spec.case_PZeroRTT;
      sl.base[0U] =
        LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(0U,
                0U,
                4U,
                0U),
              4U,
              6U,
              1U),
            6U,
            7U,
            1U),
          7U,
          8U,
          1U);
      uint32_t len1 = 1U;
      uint32_t pos1 = 0U + len1;
      store32_be(sl.base + pos1, version);
      uint32_t len = 4U;
      uint32_t pos11 = pos1 + len;
      uint32_t pout_payload0 = pos11 + 1U;
      uint8_t *payload = sl.base + pout_payload0;
      memcpy(payload, dcid, dcil * sizeof (uint8_t));
      sl.base[pos11] = (uint8_t)dcil;
      uint32_t len2 = 1U;
      uint32_t pos_payload = pos11 + len2;
      uint32_t pos12 = pos_payload + dcil;
      uint32_t pout_payload = pos12 + 1U;
      uint8_t *payload0 = sl.base + pout_payload;
      memcpy(payload0, scid, scil * sizeof (uint8_t));
      sl.base[pos12] = (uint8_t)scil;
      uint32_t len3 = 1U;
      uint32_t pos_payload0 = pos12 + len3;
      uint32_t pos2 = pos_payload0 + scil;
      uint32_t pos20 = pos2;
      uint32_t pos110 = pos20;
      uint8_t *base = sl.base;
      uint32_t len4 = write_varint(payload_and_pn_length, (uint8_t *)base, pos110);
      uint32_t res = pos110 + len4;
      uint32_t pos21 = res;
      uint32_t pos22 = pos21;
      len0 = pos22;
    }
    else if (spec.tag == PHandshake)
    {
      uint64_t payload_and_pn_length = spec.case_PHandshake;
      sl.base[0U] =
        LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(0U,
                0U,
                4U,
                0U),
              4U,
              6U,
              2U),
            6U,
            7U,
            1U),
          7U,
          8U,
          1U);
      uint32_t len1 = 1U;
      uint32_t pos1 = 0U + len1;
      store32_be(sl.base + pos1, version);
      uint32_t len = 4U;
      uint32_t pos11 = pos1 + len;
      uint32_t pout_payload0 = pos11 + 1U;
      uint8_t *payload = sl.base + pout_payload0;
      memcpy(payload, dcid, dcil * sizeof (uint8_t));
      sl.base[pos11] = (uint8_t)dcil;
      uint32_t len2 = 1U;
      uint32_t pos_payload = pos11 + len2;
      uint32_t pos12 = pos_payload + dcil;
      uint32_t pout_payload = pos12 + 1U;
      uint8_t *payload0 = sl.base + pout_payload;
      memcpy(payload0, scid, scil * sizeof (uint8_t));
      sl.base[pos12] = (uint8_t)scil;
      uint32_t len3 = 1U;
      uint32_t pos_payload0 = pos12 + len3;
      uint32_t pos2 = pos_payload0 + scil;
      uint32_t pos20 = pos2;
      uint32_t pos110 = pos20;
      uint8_t *base = sl.base;
      uint32_t len4 = write_varint(payload_and_pn_length, (uint8_t *)base, pos110);
      uint32_t res = pos110 + len4;
      uint32_t pos21 = res;
      uint32_t pos22 = pos21;
      len0 = pos22;
    }
    else if (spec.tag == PRetry)
    {
      uint32_t odcil = spec.case_PRetry.odcil;
      uint8_t *odcid = spec.case_PRetry.odcid;
      sl.base[0U] =
        LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(LowParse_BitFields_set_bitfield_gen8(0U,
                0U,
                4U,
                0U),
              4U,
              6U,
              3U),
            6U,
            7U,
            1U),
          7U,
          8U,
          1U);
      uint32_t len1 = 1U;
      uint32_t pos1 = 0U + len1;
      store32_be(sl.base + pos1, version);
      uint32_t len = 4U;
      uint32_t pos11 = pos1 + len;
      uint32_t pout_payload0 = pos11 + 1U;
      uint8_t *payload = sl.base + pout_payload0;
      memcpy(payload, dcid, dcil * sizeof (uint8_t));
      sl.base[pos11] = (uint8_t)dcil;
      uint32_t len2 = 1U;
      uint32_t pos_payload = pos11 + len2;
      uint32_t pos12 = pos_payload + dcil;
      uint32_t pout_payload1 = pos12 + 1U;
      uint8_t *payload0 = sl.base + pout_payload1;
      memcpy(payload0, scid, scil * sizeof (uint8_t));
      sl.base[pos12] = (uint8_t)scil;
      uint32_t len3 = 1U;
      uint32_t pos_payload0 = pos12 + len3;
      uint32_t pos2 = pos_payload0 + scil;
      uint32_t pos20 = pos2;
      uint32_t pos110 = pos20;
      uint32_t pout_payload = pos110 + 1U;
      uint8_t *payload1 = sl.base + pout_payload;
      memcpy(payload1, odcid, odcil * sizeof (uint8_t));
      sl.base[pos110] = (uint8_t)odcil;
      uint32_t len4 = 1U;
      uint32_t pos_payload1 = pos110 + len4;
      uint32_t pos21 = pos_payload1 + odcil;
      uint32_t pos22 = pos21;
      len0 = pos22;
    }
    else
      len0 = KRML_EABORT(uint32_t, "unreachable (pattern matches are exhaustive in F*)");
  }
  else
    len0 = KRML_EABORT(uint32_t, "unreachable (pattern matches are exhaustive in F*)");
  uint32_t len = len0;
  uint8_t *bs = out;
  uint8_t x = bs[0U];
  uint8_t y;
  if (uu___is_PShort(h))
  {
    uint8_t op0 = 0xFFU;
    uint8_t op1 = (uint32_t)op0 >> 3U;
    uint8_t op2 = (uint32_t)op1 << 0U;
    uint8_t op3 = (uint32_t)op2 ^ 0xFFU;
    uint8_t op4 = (uint32_t)x & (uint32_t)op3;
    uint8_t op5 = (uint32_t)pb << 0U;
    y = (uint32_t)op4 | (uint32_t)op5;
  }
  else
  {
    uint8_t op0 = 0xFFU;
    uint8_t op1 = (uint32_t)op0 >> 4U;
    uint8_t op2 = (uint32_t)op1 << 0U;
    uint8_t op3 = (uint32_t)op2 ^ 0xFFU;
    uint8_t op4 = (uint32_t)x & (uint32_t)op3;
    uint8_t op5 = (uint32_t)pb << 0U;
    y = (uint32_t)op4 | (uint32_t)op5;
  }
  uint8_t *b_ = bs;
  b_[0U] = y;
  return len;
}

static uint32_t header_len(header h)
{
  if (h.tag == PShort)
  {
    uint32_t cid_len = h.case_PShort.cid_len;
    return 1U + cid_len;
  }
  else if (h.tag == PLong)
  {
    long_header_specifics spec = h.case_PLong.spec;
    uint32_t scil = h.case_PLong.scil;
    uint32_t dcil = h.case_PLong.dcil;
    uint32_t ite;
    if (spec.tag == PInitial)
    {
      uint32_t token_length = spec.case_PInitial.token_length;
      uint64_t payload_and_pn_length = spec.case_PInitial.payload_and_pn_length;
      ite = varint_len((uint64_t)token_length) + token_length + varint_len(payload_and_pn_length);
    }
    else if (spec.tag == PZeroRTT)
    {
      uint64_t payload_and_pn_length = spec.case_PZeroRTT;
      ite = varint_len(payload_and_pn_length);
    }
    else if (spec.tag == PHandshake)
    {
      uint64_t payload_and_pn_length = spec.case_PHandshake;
      ite = varint_len(payload_and_pn_length);
    }
    else if (spec.tag == PRetry)
    {
      uint32_t odcil = spec.case_PRetry.odcil;
      ite = 1U + odcil;
    }
    else
      ite = KRML_EABORT(uint32_t, "unreachable (pattern matches are exhaustive in F*)");
    return 7U + dcil + scil + ite;
  }
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static uint8_t
impl_short_protected_bits(uint8_t reserved_bits, uint8_t key_phase, uint32_t pnl)
{
  uint8_t pnl_1 = (uint32_t)(uint8_t)pnl - 1U;
  uint8_t op0 = 0xFFU;
  uint8_t op1 = (uint32_t)op0 >> 6U;
  uint8_t op2 = (uint32_t)op1 << 3U;
  uint8_t op3 = (uint32_t)op2 ^ 0xFFU;
  uint8_t op01 = 0xFFU;
  uint8_t op11 = (uint32_t)op01 >> 7U;
  uint8_t op21 = (uint32_t)op11 << 2U;
  uint8_t op31 = (uint32_t)op21 ^ 0xFFU;
  uint8_t op02 = 0xFFU;
  uint8_t op12 = (uint32_t)op02 >> 6U;
  uint8_t op22 = (uint32_t)op12 << 0U;
  uint8_t op32 = (uint32_t)op22 ^ 0xFFU;
  uint8_t op4 = 0U & (uint32_t)op32;
  uint8_t op50 = (uint32_t)pnl_1 << 0U;
  uint8_t op40 = ((uint32_t)op4 | (uint32_t)op50) & (uint32_t)op31;
  uint8_t op51 = (uint32_t)key_phase << 2U;
  uint8_t op41 = ((uint32_t)op40 | (uint32_t)op51) & (uint32_t)op3;
  uint8_t op5 = (uint32_t)reserved_bits << 3U;
  return (uint32_t)op41 | (uint32_t)op5;
}

static uint8_t impl_long_protected_bits(uint8_t reserved_bits, uint32_t pnl)
{
  uint8_t pnl_1 = (uint32_t)(uint8_t)pnl - 1U;
  uint8_t op0 = 0xFFU;
  uint8_t op1 = (uint32_t)op0 >> 6U;
  uint8_t op2 = (uint32_t)op1 << 2U;
  uint8_t op3 = (uint32_t)op2 ^ 0xFFU;
  uint8_t op01 = 0xFFU;
  uint8_t op11 = (uint32_t)op01 >> 6U;
  uint8_t op21 = (uint32_t)op11 << 0U;
  uint8_t op31 = (uint32_t)op21 ^ 0xFFU;
  uint8_t op4 = 0U & (uint32_t)op31;
  uint8_t op50 = (uint32_t)pnl_1 << 0U;
  uint8_t op40 = ((uint32_t)op4 | (uint32_t)op50) & (uint32_t)op3;
  uint8_t op5 = (uint32_t)reserved_bits << 2U;
  return (uint32_t)op40 | (uint32_t)op5;
}

static header p_header(EverQuic_header h)
{
  if (h.tag == EverQuic_BShort)
  {
    uint32_t pnl = h.case_BShort.packet_number_length;
    uint32_t cid_len = h.case_BShort.cid_len;
    uint8_t *cid = h.case_BShort.cid;
    uint8_t phase = h.case_BShort.phase;
    bool spin = h.case_BShort.spin;
    uint8_t rb = h.case_BShort.reserved_bits;
    return
      (
        (header){
          .tag = PShort,
          {
            .case_PShort = {
              .protected_bits = impl_short_protected_bits(rb, phase, pnl),
              .spin = spin,
              .cid = cid,
              .cid_len = cid_len
            }
          }
        }
      );
  }
  else if (h.tag == EverQuic_BLong)
  {
    EverQuic_long_header_specifics spec = h.case_BLong.spec;
    uint32_t scil = h.case_BLong.scil;
    uint8_t *scid = h.case_BLong.scid;
    uint32_t dcil = h.case_BLong.dcil;
    uint8_t *dcid = h.case_BLong.dcid;
    uint32_t version = h.case_BLong.version;
    if (spec.tag == EverQuic_BInitial)
    {
      uint32_t token_length = spec.case_BInitial.token_length;
      uint8_t *token = spec.case_BInitial.token;
      uint32_t pnl = spec.case_BInitial.packet_number_length;
      uint64_t payload_and_pn_length = spec.case_BInitial.payload_and_pn_length;
      uint8_t rb = spec.case_BInitial.reserved_bits;
      long_header_specifics
      spec_ =
        {
          .tag = PInitial,
          {
            .case_PInitial = {
              .payload_and_pn_length = payload_and_pn_length,
              .token = token,
              .token_length = token_length
            }
          }
        };
      return
        (
          (header){
            .tag = PLong,
            {
              .case_PLong = {
                .protected_bits = impl_long_protected_bits(rb, pnl), .version = version,
                .dcid = dcid, .dcil = dcil, .scid = scid, .scil = scil, .spec = spec_
              }
            }
          }
        );
    }
    else if (spec.tag == EverQuic_BZeroRTT)
    {
      uint32_t pnl = spec.case_BZeroRTT.packet_number_length;
      uint64_t payload_and_pn_length = spec.case_BZeroRTT.payload_and_pn_length;
      uint8_t rb = spec.case_BZeroRTT.reserved_bits;
      long_header_specifics spec_ = { .tag = PZeroRTT, { .case_PZeroRTT = payload_and_pn_length } };
      return
        (
          (header){
            .tag = PLong,
            {
              .case_PLong = {
                .protected_bits = impl_long_protected_bits(rb, pnl), .version = version,
                .dcid = dcid, .dcil = dcil, .scid = scid, .scil = scil, .spec = spec_
              }
            }
          }
        );
    }
    else if (spec.tag == EverQuic_BHandshake)
    {
      uint32_t pnl = spec.case_BHandshake.packet_number_length;
      uint64_t payload_and_pn_length = spec.case_BHandshake.payload_and_pn_length;
      uint8_t rb = spec.case_BHandshake.reserved_bits;
      long_header_specifics
      spec_ = { .tag = PHandshake, { .case_PHandshake = payload_and_pn_length } };
      return
        (
          (header){
            .tag = PLong,
            {
              .case_PLong = {
                .protected_bits = impl_long_protected_bits(rb, pnl), .version = version,
                .dcid = dcid, .dcil = dcil, .scid = scid, .scil = scil, .spec = spec_
              }
            }
          }
        );
    }
    else if (spec.tag == EverQuic_BRetry)
    {
      uint32_t odcil = spec.case_BRetry.odcil;
      uint8_t *odcid = spec.case_BRetry.odcid;
      uint8_t unused = spec.case_BRetry.unused;
      long_header_specifics
      spec_ = { .tag = PRetry, { .case_PRetry = { .odcid = odcid, .odcil = odcil } } };
      return
        (
          (header){
            .tag = PLong,
            {
              .case_PLong = {
                .protected_bits = unused, .version = version, .dcid = dcid, .dcil = dcil,
                .scid = scid, .scil = scil, .spec = spec_
              }
            }
          }
        );
    }
    else
    {
      KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
        __FILE__,
        __LINE__,
        "unreachable (pattern matches are exhaustive in F*)");
      KRML_HOST_EXIT(255U);
    }
  }
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static uint64_t last_pn(bool is_retry, uint64_t pn)
{
  if (is_retry)
    return 0ULL;
  else
  {
    uint64_t cond = secrets_are_equal_62(pn, 0ULL) ^ (uint64_t)1U;
    return pn - cond;
  }
}

static void write_header0(EverQuic_header h, uint64_t pn, uint8_t *out, uint32_t out_len)
{
  EverQuic_dcid_len(h);
  last_pn(EverQuic_is_retry(h), pn);
  header ph = p_header(h);
  uint32_t len = write_header(ph, out, out_len);
  if (!EverQuic_is_retry(h))
  {
    uint32_t pn_len = EverQuic_pn_length(h);
    uint8_t *bs = out + len;
    write_packet_number(pn_len, pn, bs);
  }
}

#define None 0
#define Some 1

typedef uint8_t option__uint32_t_tags;

typedef struct option__uint32_t_s
{
  option__uint32_t_tags tag;
  uint32_t v;
}
option__uint32_t;

static option__uint32_t putative_pn_offset(uint32_t cid_len, uint8_t *b, uint32_t b_len)
{
  LowParse_Slice_slice input = { .base = b, .len = b_len };
  uint64_t res = validate_header(cid_len, input, 0ULL);
  if (LowParse_Low_ErrorCode_is_error(res))
    return ((option__uint32_t){ .tag = None });
  else
    return ((option__uint32_t){ .tag = Some, .v = (uint32_t)res });
}

static uint32_t impl_protected_bits_pn_length(uint8_t pb)
{
  uint8_t op1 = (uint32_t)pb << 6U;
  uint8_t bf = (uint32_t)op1 >> 6U;
  return (uint32_t)(1U + (uint32_t)bf);
}

static uint8_t impl_protected_bits_reserved(bool is_short, uint8_t pb)
{
  if (is_short)
  {
    uint8_t op1 = (uint32_t)pb << 3U;
    return (uint32_t)op1 >> 6U;
  }
  else
  {
    uint8_t op1 = (uint32_t)pb << 4U;
    return (uint32_t)op1 >> 6U;
  }
}

static uint8_t impl_protected_bits_key_phase(uint8_t x)
{
  uint8_t op1 = (uint32_t)x << 5U;
  return (uint32_t)op1 >> 7U;
}

static EverQuic_header header_p(header h)
{
  if (h.tag == PShort)
  {
    uint32_t cid_len = h.case_PShort.cid_len;
    uint8_t *cid = h.case_PShort.cid;
    bool spin = h.case_PShort.spin;
    uint8_t pb = h.case_PShort.protected_bits;
    uint8_t rb = impl_protected_bits_reserved(true, pb);
    uint8_t phase = impl_protected_bits_key_phase(pb);
    uint32_t pnl = impl_protected_bits_pn_length(pb);
    return
      (
        (EverQuic_header){
          .tag = EverQuic_BShort,
          {
            .case_BShort = {
              .reserved_bits = rb, .spin = spin, .phase = phase, .cid = cid, .cid_len = cid_len,
              .packet_number_length = pnl
            }
          }
        }
      );
  }
  else if (h.tag == PLong)
  {
    long_header_specifics spec = h.case_PLong.spec;
    uint32_t scil = h.case_PLong.scil;
    uint8_t *scid = h.case_PLong.scid;
    uint32_t dcil = h.case_PLong.dcil;
    uint8_t *dcid = h.case_PLong.dcid;
    uint32_t version = h.case_PLong.version;
    uint8_t pb = h.case_PLong.protected_bits;
    uint8_t rb = impl_protected_bits_reserved(false, pb);
    uint32_t pnl = impl_protected_bits_pn_length(pb);
    EverQuic_long_header_specifics ite;
    if (spec.tag == PInitial)
    {
      uint32_t token_length = spec.case_PInitial.token_length;
      uint8_t *token = spec.case_PInitial.token;
      uint64_t payload_and_pn_length = spec.case_PInitial.payload_and_pn_length;
      ite =
        (
          (EverQuic_long_header_specifics){
            .tag = EverQuic_BInitial,
            {
              .case_BInitial = {
                .reserved_bits = rb, .payload_and_pn_length = payload_and_pn_length,
                .packet_number_length = pnl, .token = token, .token_length = token_length
              }
            }
          }
        );
    }
    else if (spec.tag == PHandshake)
    {
      uint64_t payload_and_pn_length = spec.case_PHandshake;
      ite =
        (
          (EverQuic_long_header_specifics){
            .tag = EverQuic_BHandshake,
            {
              .case_BHandshake = {
                .reserved_bits = rb,
                .payload_and_pn_length = payload_and_pn_length,
                .packet_number_length = pnl
              }
            }
          }
        );
    }
    else if (spec.tag == PZeroRTT)
    {
      uint64_t payload_and_pn_length = spec.case_PZeroRTT;
      ite =
        (
          (EverQuic_long_header_specifics){
            .tag = EverQuic_BZeroRTT,
            {
              .case_BZeroRTT = {
                .reserved_bits = rb,
                .payload_and_pn_length = payload_and_pn_length,
                .packet_number_length = pnl
              }
            }
          }
        );
    }
    else if (spec.tag == PRetry)
    {
      uint32_t odcil = spec.case_PRetry.odcil;
      uint8_t *odcid = spec.case_PRetry.odcid;
      ite =
        (
          (EverQuic_long_header_specifics){
            .tag = EverQuic_BRetry,
            { .case_BRetry = { .unused = pb, .odcid = odcid, .odcil = odcil } }
          }
        );
    }
    else
      ite =
        KRML_EABORT(EverQuic_long_header_specifics,
          "unreachable (pattern matches are exhaustive in F*)");
    return
      (
        (EverQuic_header){
          .tag = EverQuic_BLong,
          {
            .case_BLong = {
              .version = version, .dcid = dcid, .dcil = dcil, .scid = scid, .scil = scil,
              .spec = ite
            }
          }
        }
      );
  }
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

typedef struct __QUIC_Impl_Header_Base_header_uint64_t_s
{
  EverQuic_header fst;
  uint64_t snd;
}
__QUIC_Impl_Header_Base_header_uint64_t;

static __QUIC_Impl_Header_Base_header_uint64_t
read_header0(uint8_t *packet, uint32_t packet_len, uint32_t cid_len, uint64_t last)
{
  header ph = read_header(packet, packet_len, cid_len);
  EverQuic_header h = header_p(ph);
  if (is_retry(ph))
    return ((__QUIC_Impl_Header_Base_header_uint64_t){ .fst = h, .snd = last });
  else
  {
    uint32_t len = header_len(ph);
    uint32_t pn_len = EverQuic_pn_length(h);
    uint8_t *bs = packet + len;
    uint64_t pn = read_packet_number(last, pn_len, bs);
    return ((__QUIC_Impl_Header_Base_header_uint64_t){ .fst = h, .snd = pn });
  }
}

static uint32_t pn_sizemask_ct_num(uint32_t pn_len)
{
  uint32_t pn_len_1 = pn_len - 1U;
  return
    secrets_are_equal_32_2(pn_len_1,
      0U)
    * 4278190080U
    + secrets_are_equal_32_2(pn_len_1, 1U) * 4294901760U
    + secrets_are_equal_32_2(pn_len_1, 2U) * 4294967040U
    + secrets_are_equal_32_2(pn_len_1, 3U) * 4294967295U;
}

static void
header_encrypt_ct_secret_preserving_not_retry(
  Spec_Agile_AEAD_alg a,
  NotEverCrypt_CTR_state_s *s,
  uint8_t *k,
  bool is_short,
  uint32_t pn_offset,
  uint32_t pn_len,
  uint8_t *dst
)
{
  uint8_t mask[16U] = { 0U };
  uint8_t pn_sm[4U] = { 0U };
  uint8_t *sample = dst + pn_offset + 4U;
  uint8_t zeroes_[64U] = { 0U };
  uint8_t *zeroes = zeroes_;
  uint8_t dst_block_[64U] = { 0U };
  uint8_t *dst_block = dst_block_;
  switch (Spec_Agile_AEAD_cipher_alg_of_supported_alg(a))
  {
    case Spec_Agile_Cipher_CHACHA20:
      {
        uint32_t ctr = load32_le(sample);
        uint8_t *iv = sample + 4U;
        NotEverCrypt_CTR_init(s, k, iv, 12U, ctr);
        NotEverCrypt_CTR_update_block(s, dst_block, zeroes);
        uint8_t *dst_slice = dst_block;
        memcpy(mask, dst_slice, 16U * sizeof (uint8_t));
        break;
      }
    default:
      {
        uint32_t ctr = load32_be(sample + 12U);
        uint8_t *iv = sample;
        NotEverCrypt_CTR_init(s, k, iv, 12U, ctr);
        NotEverCrypt_CTR_update_block(s, dst_block, zeroes);
        uint8_t *dst_slice = dst_block;
        memcpy(mask, dst_slice, 16U * sizeof (uint8_t));
      }
  }
  uint32_t x = pn_sizemask_ct_num(pn_len);
  store32_be(pn_sm, x);
  uint8_t *pnmask = mask + 1U;
  uint8_t *dst10 = pnmask;
  for (uint32_t i = 0U; i < 4U; i++)
  {
    uint8_t xi = dst10[i];
    uint8_t yi = pn_sm[i];
    dst10[i] = (uint32_t)xi & (uint32_t)yi;
  }
  uint32_t protected_bits;
  if (is_short)
    protected_bits = 5U;
  else
    protected_bits = 4U;
  uint8_t mask_0 = mask[0U];
  uint8_t f_ = dst[0U];
  uint8_t f_logxor = (uint32_t)f_ ^ (uint32_t)mask_0;
  uint8_t op1 = (uint32_t)f_logxor << (8U - protected_bits);
  uint8_t f_get_bf = (uint32_t)op1 >> (8U - protected_bits + 0U);
  uint8_t op0 = 0xFFU;
  uint8_t op10 = (uint32_t)op0 >> (8U - (protected_bits - 0U));
  uint8_t op2 = (uint32_t)op10 << 0U;
  uint8_t op3 = (uint32_t)op2 ^ 0xFFU;
  uint8_t op4 = (uint32_t)f_ & (uint32_t)op3;
  uint8_t op5 = (uint32_t)f_get_bf << 0U;
  uint8_t f_0 = (uint32_t)op4 | (uint32_t)op5;
  uint8_t *dst1 = dst + pn_offset;
  for (uint32_t i = 0U; i < 4U; i++)
  {
    uint8_t xi = dst1[i];
    uint8_t yi = pnmask[i];
    dst1[i] = (uint32_t)xi ^ (uint32_t)yi;
  }
  dst[0U] = f_0;
}

static void
header_encrypt(
  Spec_Agile_AEAD_alg a,
  NotEverCrypt_CTR_state_s *s,
  uint8_t *k,
  uint8_t *dst,
  bool is_short,
  bool is_retry,
  uint32_t public_len,
  uint32_t pn_len
)
{
  if (!is_retry)
  {
    uint8_t *bs = dst;
    header_encrypt_ct_secret_preserving_not_retry(a, s, k, is_short, public_len, pn_len, bs);
  }
}

#define H_Failure 0
#define H_Success 1

typedef uint8_t h_result_tags;

typedef struct h_result_s
{
  h_result_tags tag;
  EverQuic_header h;
  uint64_t pn;
  uint32_t cipher_length;
}
h_result;

static void
header_decrypt_aux_ct_secret_preserving_not_retry(
  Spec_Agile_AEAD_alg a,
  NotEverCrypt_CTR_state_s *s,
  uint8_t *k,
  bool is_short,
  uint32_t pn_offset,
  uint8_t *dst
)
{
  uint8_t *bs = dst;
  uint8_t f = bs[0U];
  uint32_t sample_offset = pn_offset + 4U;
  uint8_t *sample = bs + sample_offset;
  uint8_t mask[16U] = { 0U };
  uint8_t pn_sm[4U] = { 0U };
  uint8_t zeroes_[64U] = { 0U };
  uint8_t *zeroes = zeroes_;
  uint8_t dst_block_[64U] = { 0U };
  uint8_t *dst_block = dst_block_;
  switch (Spec_Agile_AEAD_cipher_alg_of_supported_alg(a))
  {
    case Spec_Agile_Cipher_CHACHA20:
      {
        uint32_t ctr = load32_le(sample);
        uint8_t *iv = sample + 4U;
        NotEverCrypt_CTR_init(s, k, iv, 12U, ctr);
        NotEverCrypt_CTR_update_block(s, dst_block, zeroes);
        uint8_t *dst_slice = dst_block;
        memcpy(mask, dst_slice, 16U * sizeof (uint8_t));
        break;
      }
    default:
      {
        uint32_t ctr = load32_be(sample + 12U);
        uint8_t *iv = sample;
        NotEverCrypt_CTR_init(s, k, iv, 12U, ctr);
        NotEverCrypt_CTR_update_block(s, dst_block, zeroes);
        uint8_t *dst_slice = dst_block;
        memcpy(mask, dst_slice, 16U * sizeof (uint8_t));
      }
  }
  uint32_t protected_bits;
  if (is_short)
    protected_bits = 5U;
  else
    protected_bits = 4U;
  uint8_t mask0 = mask[0U];
  uint8_t f_logxor = (uint32_t)f ^ (uint32_t)mask0;
  uint8_t op1 = (uint32_t)f_logxor << (8U - protected_bits);
  uint8_t f_bf = (uint32_t)op1 >> (8U - protected_bits + 0U);
  uint8_t op0 = 0xFFU;
  uint8_t op10 = (uint32_t)op0 >> (8U - (protected_bits - 0U));
  uint8_t op2 = (uint32_t)op10 << 0U;
  uint8_t op3 = (uint32_t)op2 ^ 0xFFU;
  uint8_t op4 = (uint32_t)f & (uint32_t)op3;
  uint8_t op5 = (uint32_t)f_bf << 0U;
  uint8_t f_ = (uint32_t)op4 | (uint32_t)op5;
  bs[0U] = f_;
  uint8_t op11 = (uint32_t)f_ << 6U;
  uint8_t pn_len = (uint32_t)op11 >> 6U;
  uint32_t x = pn_sizemask_ct_num((uint32_t)pn_len + 1U);
  store32_be(pn_sm, x);
  uint8_t *pnmask = mask + 1U;
  uint8_t *dst1 = pnmask;
  for (uint32_t i = 0U; i < 4U; i++)
  {
    uint8_t xi = dst1[i];
    uint8_t yi = pn_sm[i];
    dst1[i] = (uint32_t)xi & (uint32_t)yi;
  }
  uint8_t *dst10 = bs + pn_offset;
  for (uint32_t i = 0U; i < 4U; i++)
  {
    uint8_t xi = dst10[i];
    uint8_t yi = pnmask[i];
    dst10[i] = (uint32_t)xi ^ (uint32_t)yi;
  }
}

#define HD_Failure 0
#define HD_Success_Retry 1
#define HD_Success_NotRetry 2

typedef uint8_t header_decrypt_aux_result;

static header_decrypt_aux_result
header_decrypt_aux(
  Spec_Agile_AEAD_alg a,
  NotEverCrypt_CTR_state_s *s,
  uint8_t *k,
  uint32_t cid_len,
  uint8_t *dst,
  uint32_t dst_len
)
{
  if (dst_len == 0U)
    return HD_Failure;
  else
  {
    uint8_t f = dst[0U];
    uint8_t short_or_long = LowParse_BitFields_get_bitfield_gen8(f, 7U, 8U);
    bool isshort = short_or_long == 0U;
    uint8_t retry_or_other = LowParse_BitFields_get_bitfield_gen8(f, 4U, 6U);
    bool isretry = !isshort && retry_or_other == 3U;
    if (isretry)
      return HD_Success_Retry;
    else
    {
      option__uint32_t scrut = putative_pn_offset(cid_len, dst, dst_len);
      if (scrut.tag == None)
        return HD_Failure;
      else if (scrut.tag == Some)
      {
        uint32_t pn_offset = scrut.v;
        if (dst_len < pn_offset + 20U)
          return HD_Failure;
        else
        {
          header_decrypt_aux_ct_secret_preserving_not_retry(a, s, k, isshort, pn_offset, dst);
          return HD_Success_NotRetry;
        }
      }
      else
      {
        KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
          __FILE__,
          __LINE__,
          "unreachable (pattern matches are exhaustive in F*)");
        KRML_HOST_EXIT(255U);
      }
    }
  }
}

static uint64_t max_cipher_length64 = 4294950796ULL;

static bool uu___is_None__uint32_t(option__uint32_t projectee)
{
  if (projectee.tag == None)
    return true;
  else
    return false;
}

static h_result
header_decrypt(
  Spec_Agile_AEAD_alg a,
  NotEverCrypt_CTR_state_s *s,
  uint8_t *k,
  uint32_t cid_len,
  uint64_t last,
  uint8_t *dst,
  uint32_t dst_len
)
{
  header_decrypt_aux_result aux = header_decrypt_aux(a, s, k, cid_len, dst, dst_len);
  switch (aux)
  {
    case HD_Failure:
      {
        return ((h_result){ .tag = H_Failure });
      }
    case HD_Success_Retry:
      {
        if (uu___is_None__uint32_t(putative_pn_offset(cid_len, dst, dst_len)))
          return ((h_result){ .tag = H_Failure });
        else
        {
          __QUIC_Impl_Header_Base_header_uint64_t scrut = read_header0(dst, dst_len, cid_len, last);
          EverQuic_header h = scrut.fst;
          uint64_t pn = scrut.snd;
          return ((h_result){ .tag = H_Success, .h = h, .pn = pn, .cipher_length = 0U });
        }
        break;
      }
    default:
      {
        __QUIC_Impl_Header_Base_header_uint64_t scrut = read_header0(dst, dst_len, cid_len, last);
        EverQuic_header h = scrut.fst;
        uint64_t pn = scrut.snd;
        uint32_t hlen = EverQuic_header_len(h);
        uint32_t rlen = dst_len - hlen;
        uint64_t rlen64 = (uint64_t)rlen;
        uint64_t clen64;
        if (EverQuic_has_payload_length(h))
          clen64 = EverQuic_payload_length(h);
        else
          clen64 = rlen64;
        uint64_t
        clen64_checked = max640(min640(min640(clen64, rlen64), max_cipher_length64 - 1ULL), 16ULL);
        uint32_t clen32 = (uint32_t)clen64_checked;
        return ((h_result){ .tag = H_Success, .h = h, .pn = pn, .cipher_length = clen32 });
      }
  }
}

static uint8_t label_key[3U] = { 0x6bU, 0x65U, 0x79U };

static uint8_t label_iv[2U] = { 0x69U, 0x76U };

static uint8_t label_hp[2U] = { 0x68U, 0x70U };

static uint8_t
prefix[11U] = { 0x74U, 0x6cU, 0x73U, 0x31U, 0x33U, 0x20U, 0x71U, 0x75U, 0x69U, 0x63U, 0x20U };

static void
derive_secret(
  Spec_Hash_Definitions_hash_alg a,
  uint8_t *dst,
  uint8_t dst_len,
  uint8_t *secret,
  uint8_t *label,
  uint8_t label_len
)
{
  uint32_t label_len32 = (uint32_t)label_len;
  uint32_t dst_len32 = (uint32_t)dst_len;
  uint32_t info_len = 14U + label_len32 + 1U;
  KRML_CHECK_SIZE(sizeof (uint8_t), info_len);
  uint8_t info[info_len];
  memset(info, 0U, info_len * sizeof (uint8_t));
  uint8_t *info_lb = info + 1U;
  uint8_t *info_llen = info + 2U;
  uint8_t *info_prefix = info + 3U;
  uint8_t *info_label = info + 14U;
  info_lb[0U] = dst_len;
  info_llen[0U] = (uint32_t)label_len + 11U;
  memcpy(info_prefix, prefix, 11U * sizeof (uint8_t));
  memcpy(info_label, label, label_len32 * sizeof (uint8_t));
  uint8_t *bs = info;
  uint32_t sw;
  switch (a)
  {
    case Spec_Hash_Definitions_MD5:
      {
        sw = 16U;
        break;
      }
    case Spec_Hash_Definitions_SHA1:
      {
        sw = 20U;
        break;
      }
    case Spec_Hash_Definitions_SHA2_224:
      {
        sw = 28U;
        break;
      }
    case Spec_Hash_Definitions_SHA2_256:
      {
        sw = 32U;
        break;
      }
    case Spec_Hash_Definitions_SHA2_384:
      {
        sw = 48U;
        break;
      }
    case Spec_Hash_Definitions_SHA2_512:
      {
        sw = 64U;
        break;
      }
    case Spec_Hash_Definitions_Blake2S:
      {
        sw = 32U;
        break;
      }
    case Spec_Hash_Definitions_Blake2B:
      {
        sw = 64U;
        break;
      }
    case Spec_Hash_Definitions_SHA3_224:
      {
        sw = 28U;
        break;
      }
    case Spec_Hash_Definitions_SHA3_256:
      {
        sw = 32U;
        break;
      }
    case Spec_Hash_Definitions_SHA3_384:
      {
        sw = 48U;
        break;
      }
    case Spec_Hash_Definitions_SHA3_512:
      {
        sw = 64U;
        break;
      }
    default:
      {
        KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
        KRML_HOST_EXIT(253U);
      }
  }
  EverCrypt_HKDF_expand(a, dst, secret, sw, bs, info_len, dst_len32);
}

static void iv_for_encrypt_decrypt(uint8_t *siv, uint8_t *dst, uint64_t pn)
{
  memset(dst, 0U, 12U * sizeof (uint8_t));
  uint8_t *pnb_store = dst + 4U;
  store64_be(pnb_store, pn);
  uint8_t *dst1 = dst;
  for (uint32_t i = 0U; i < 12U; i++)
  {
    uint8_t xi = dst1[i];
    uint8_t yi = siv[i];
    dst1[i] = (uint32_t)xi ^ (uint32_t)yi;
  }
}

static EverCrypt_Error_error_code
payload_encrypt(
  EverCrypt_AEAD_state_s *s,
  uint8_t *siv,
  uint8_t *dst,
  uint64_t pn,
  uint32_t header_len,
  uint8_t *plain,
  uint32_t plain_len
)
{
  uint8_t iv[12U] = { 0U };
  uint8_t *aad = dst;
  uint8_t *cipher = dst + header_len;
  uint8_t *tag = dst + header_len + plain_len;
  iv_for_encrypt_decrypt(siv, iv, pn);
  EverCrypt_Error_error_code
  res = EverCrypt_AEAD_encrypt(s, iv, 12U, aad, header_len, plain, plain_len, cipher, tag);
  return res;
}

static EverCrypt_Error_error_code
encrypt(
  Spec_Agile_AEAD_alg a,
  EverCrypt_AEAD_state_s *aead,
  uint8_t *siv,
  NotEverCrypt_CTR_state_s *ctr,
  uint8_t *hpk,
  uint8_t *dst,
  EverQuic_header h,
  uint64_t pn,
  uint8_t *plain,
  uint32_t plain_len
)
{
  uint32_t header_len = EverQuic_header_len(h);
  bool isretry = EverQuic_is_retry(h);
  uint32_t ite;
  if (isretry)
    ite = 0U;
  else
    ite = plain_len + 16U;
  write_header0(h, pn, dst, header_len + ite);
  if (isretry)
  {
    uint32_t dummy_pn_len = 1U;
    header_encrypt(a, ctr, hpk, dst, false, true, EverQuic_public_header_len(h), dummy_pn_len);
    return EverCrypt_Error_Success;
  }
  else
  {
    uint32_t pn_len = EverQuic_pn_length(h);
    uint8_t *bs = dst;
    EverCrypt_Error_error_code
    res = payload_encrypt(aead, siv, bs, pn, header_len, plain, plain_len);
    EverCrypt_Error_error_code res0 = res;
    switch (res0)
    {
      case EverCrypt_Error_Success:
        {
          header_encrypt(a,
            ctr,
            hpk,
            dst,
            EverQuic_uu___is_BShort(h),
            isretry,
            EverQuic_public_header_len(h),
            pn_len);
          return res0;
        }
      default:
        {
          return res0;
        }
    }
  }
}

static EverCrypt_Error_error_code
payload_decrypt(
  EverCrypt_AEAD_state_s *s,
  uint8_t *siv,
  uint8_t *dst,
  uint32_t hlen,
  uint64_t pn,
  uint32_t cipher_and_tag_len
)
{
  uint8_t iv[12U] = { 0U };
  uint32_t cipher_len = cipher_and_tag_len - 16U;
  uint8_t *aad = dst;
  uint8_t *cipher = dst + hlen;
  uint8_t *tag = dst + hlen + cipher_len;
  iv_for_encrypt_decrypt(siv, iv, pn);
  EverCrypt_Error_error_code
  res = EverCrypt_AEAD_decrypt(s, iv, 12U, aad, hlen, cipher, cipher_len, tag, cipher);
  return res;
}

static EverCrypt_Error_error_code
decrypt(
  Spec_Agile_AEAD_alg a,
  EverCrypt_AEAD_state_s *aead,
  uint8_t *siv,
  NotEverCrypt_CTR_state_s *ctr,
  uint8_t *hpk,
  uint8_t *dst,
  uint32_t dst_len,
  EverQuic_result *dst_hdr,
  uint64_t last_pn,
  uint32_t cid_len
)
{
  h_result scrut = header_decrypt(a, ctr, hpk, cid_len, last_pn, dst, dst_len);
  if (scrut.tag == H_Failure)
    return EverCrypt_Error_DecodeError;
  else if (scrut.tag == H_Success)
  {
    uint32_t cipher_and_tag_len = scrut.cipher_length;
    uint64_t pn = scrut.pn;
    EverQuic_header h = scrut.h;
    uint32_t hlen = EverQuic_header_len(h);
    if (EverQuic_is_retry(h))
    {
      EverQuic_result
      r = { .pn = pn, .header = h, .header_len = hlen, .plain_len = 0U, .total_len = hlen };
      dst_hdr[0U] = r;
      return EverCrypt_Error_Success;
    }
    else
    {
      EverQuic_pn_length(h);
      uint32_t plain_len = cipher_and_tag_len - 16U;
      uint8_t *bs = dst;
      EverCrypt_Error_error_code res = payload_decrypt(aead, siv, bs, hlen, pn, cipher_and_tag_len);
      EverCrypt_Error_error_code res0 = res;
      EverQuic_result
      r =
        {
          .pn = pn, .header = h, .header_len = hlen, .plain_len = plain_len,
          .total_len = hlen + cipher_and_tag_len
        };
      dst_hdr[0U] = r;
      return res0;
    }
  }
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static uint8_t key_len(Spec_Agile_AEAD_alg a)
{
  switch (a)
  {
    case Spec_Agile_AEAD_AES128_GCM:
      {
        return 16U;
      }
    case Spec_Agile_AEAD_AES256_GCM:
      {
        return 32U;
      }
    case Spec_Agile_AEAD_CHACHA20_POLY1305:
      {
        return 32U;
      }
    default:
      {
        KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
        KRML_HOST_EXIT(253U);
      }
  }
}

static uint32_t key_len32(Spec_Agile_AEAD_alg a)
{
  return (uint32_t)key_len(a);
}

typedef struct EverQuic_state_s_s
{
  Spec_Hash_Definitions_hash_alg the_hash_alg;
  Spec_Agile_AEAD_alg the_aead_alg;
  EverCrypt_AEAD_state_s *aead_state;
  uint8_t *iv;
  uint8_t *hp_key;
  uint64_t *pn;
  NotEverCrypt_CTR_state_s *ctr_state;
}
EverQuic_state_s;

bool EverQuic_uu___is_State(EverQuic_index i, EverQuic_state_s projectee)
{
  KRML_MAYBE_UNUSED_VAR(i);
  KRML_MAYBE_UNUSED_VAR(projectee);
  return true;
}

Spec_Agile_AEAD_alg EverQuic_aead_alg_of_state(EverQuic_state_s *s)
{
  return (*s).the_aead_alg;
}

Spec_Hash_Definitions_hash_alg EverQuic_hash_alg_of_state(EverQuic_state_s *s)
{
  return (*s).the_hash_alg;
}

uint64_t EverQuic_last_packet_number_of_state(EverQuic_state_s *s)
{
  uint64_t *pn = (*s).pn;
  return *pn;
}

static void
create_in_core(
  EverQuic_index i,
  EverQuic_state_s **dst,
  uint64_t initial_pn,
  uint8_t *traffic_secret,
  EverCrypt_AEAD_state_s *aead_state,
  NotEverCrypt_CTR_state_s *ctr_state
)
{
  uint8_t *iv = KRML_HOST_CALLOC(12U, sizeof (uint8_t));
  KRML_CHECK_SIZE(sizeof (uint8_t), key_len32(i.aead_alg));
  uint8_t *hp_key = KRML_HOST_CALLOC(key_len32(i.aead_alg), sizeof (uint8_t));
  uint64_t *pn = KRML_HOST_MALLOC(sizeof (uint64_t));
  pn[0U] = initial_pn;
  EverQuic_state_s
  s =
    {
      .the_hash_alg = i.hash_alg, .the_aead_alg = i.aead_alg, .aead_state = aead_state, .iv = iv,
      .hp_key = hp_key, .pn = pn, .ctr_state = ctr_state
    };
  EverQuic_state_s *s1 = KRML_HOST_MALLOC(sizeof (EverQuic_state_s));
  s1[0U] = s;
  derive_secret(i.hash_alg, iv, 12U, traffic_secret, label_iv, 2U);
  derive_secret(i.hash_alg, hp_key, key_len(i.aead_alg), traffic_secret, label_hp, 2U);
  *dst = s1;
}

EverCrypt_Error_error_code
EverQuic_create_in(
  EverQuic_index i,
  EverQuic_state_s **dst,
  uint64_t initial_pn,
  uint8_t *traffic_secret
)
{
  KRML_CHECK_SIZE(sizeof (uint8_t), key_len32(i.aead_alg));
  uint8_t aead_key[key_len32(i.aead_alg)];
  memset(aead_key, 0U, key_len32(i.aead_alg) * sizeof (uint8_t));
  EverCrypt_AEAD_state_s *aead_state = NULL;
  NotEverCrypt_CTR_state_s *ctr_state = NULL;
  uint8_t dummy_iv[12U] = { 0U };
  derive_secret(i.hash_alg, aead_key, key_len(i.aead_alg), traffic_secret, label_key, 3U);
  EverCrypt_Error_error_code ret = EverCrypt_AEAD_create_in(i.aead_alg, &aead_state, aead_key);
  EverCrypt_Error_error_code
  ret_ =
    NotEverCrypt_CTR_create_in(Spec_Agile_AEAD_cipher_alg_of_supported_alg(i.aead_alg),
      &ctr_state,
      aead_key,
      dummy_iv,
      12U,
      0U);
  switch (ret)
  {
    case EverCrypt_Error_UnsupportedAlgorithm:
      {
        return EverCrypt_Error_UnsupportedAlgorithm;
      }
    case EverCrypt_Error_Success:
      {
        switch (ret_)
        {
          case EverCrypt_Error_UnsupportedAlgorithm:
            {
              return EverCrypt_Error_UnsupportedAlgorithm;
            }
          case EverCrypt_Error_Success:
            {
              EverCrypt_AEAD_state_s *aead_state1 = aead_state;
              NotEverCrypt_CTR_state_s *ctr_state1 = ctr_state;
              create_in_core(i, dst, initial_pn, traffic_secret, aead_state1, ctr_state1);
              return EverCrypt_Error_Success;
            }
          default:
            {
              KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
              KRML_HOST_EXIT(253U);
            }
        }
        break;
      }
    default:
      {
        KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
        KRML_HOST_EXIT(253U);
      }
  }
}

EverCrypt_Error_error_code
EverQuic_encrypt(
  EverQuic_state_s *s,
  uint8_t *dst,
  uint64_t *dst_pn,
  EverQuic_header h,
  uint8_t *plain,
  uint32_t plain_len
)
{
  EverQuic_state_s scrut = *s;
  Spec_Agile_AEAD_alg aead_alg = scrut.the_aead_alg;
  EverCrypt_AEAD_state_s *aead_state = scrut.aead_state;
  uint8_t *iv = scrut.iv;
  uint8_t *hp_key = scrut.hp_key;
  uint64_t *bpn = scrut.pn;
  NotEverCrypt_CTR_state_s *ctr_state = scrut.ctr_state;
  uint64_t last_pn = *bpn;
  uint64_t pn = last_pn + 1ULL;
  bpn[0U] = pn;
  dst_pn[0U] = pn;
  return encrypt(aead_alg, aead_state, iv, ctr_state, hp_key, dst, h, pn, plain, plain_len);
}

static uint8_t
initial_salt[20U] =
  {
    0xc3U, 0xeeU, 0xf7U, 0x12U, 0xc7U, 0x2eU, 0xbbU, 0x5aU, 0x11U, 0xa7U, 0xd2U, 0x43U, 0x2bU,
    0xb4U, 0x63U, 0x65U, 0xbeU, 0xf9U, 0xf5U, 0x02U
  };

static uint8_t
server_in[9U] = { 0x73U, 0x65U, 0x72U, 0x76U, 0x65U, 0x72U, 0x20U, 0x69U, 0x6eU };

static uint8_t
client_in[9U] = { 0x63U, 0x6cU, 0x69U, 0x65U, 0x6eU, 0x74U, 0x20U, 0x69U, 0x6eU };

void
EverQuic_initial_secrets(
  uint8_t *dst_client,
  uint8_t *dst_server,
  uint8_t *cid,
  uint32_t cid_len
)
{
  uint8_t secret[32U] = { 0U };
  uint8_t tmp[20U] = { 0U };
  memcpy(tmp, initial_salt, 20U * sizeof (uint8_t));
  uint8_t *bs = tmp;
  EverCrypt_HKDF_extract(Spec_Hash_Definitions_SHA2_256, secret, bs, 20U, cid, cid_len);
  uint8_t *tmp_client_in = tmp;
  memcpy(tmp_client_in, client_in, 9U * sizeof (uint8_t));
  uint8_t *bs0 = tmp_client_in;
  EverCrypt_HKDF_expand(Spec_Hash_Definitions_SHA2_256, dst_client, secret, 32U, bs0, 9U, 32U);
  uint8_t *tmp_server_in = tmp;
  memcpy(tmp_server_in, server_in, 9U * sizeof (uint8_t));
  uint8_t *bs1 = tmp_server_in;
  EverCrypt_HKDF_expand(Spec_Hash_Definitions_SHA2_256, dst_server, secret, 32U, bs1, 9U, 32U);
}

EverCrypt_Error_error_code
EverQuic_decrypt(
  EverQuic_state_s *s,
  EverQuic_result *dst,
  uint8_t *packet,
  uint32_t len,
  uint8_t cid_len
)
{
  EverQuic_state_s scrut = *s;
  Spec_Agile_AEAD_alg aead_alg = scrut.the_aead_alg;
  EverCrypt_AEAD_state_s *aead_state = scrut.aead_state;
  uint8_t *iv = scrut.iv;
  uint8_t *hp_key = scrut.hp_key;
  uint64_t *bpn = scrut.pn;
  NotEverCrypt_CTR_state_s *ctr_state = scrut.ctr_state;
  uint64_t last_pn = *bpn;
  EverCrypt_Error_error_code
  res =
    decrypt(aead_alg,
      aead_state,
      iv,
      ctr_state,
      hp_key,
      packet,
      len,
      dst,
      last_pn,
      (uint32_t)cid_len);
  if (res == EverCrypt_Error_Success)
  {
    EverQuic_result r = dst[0U];
    uint64_t pn = r.pn;
    uint64_t pn_ = max640(last_pn, pn);
    bpn[0U] = pn_;
  }
  return res;
}

bool EverQuic_uu___is_BInitial(EverQuic_long_header_specifics projectee)
{
  if (projectee.tag == EverQuic_BInitial)
    return true;
  else
    return false;
}

bool EverQuic_uu___is_BZeroRTT(EverQuic_long_header_specifics projectee)
{
  if (projectee.tag == EverQuic_BZeroRTT)
    return true;
  else
    return false;
}

bool EverQuic_uu___is_BHandshake(EverQuic_long_header_specifics projectee)
{
  if (projectee.tag == EverQuic_BHandshake)
    return true;
  else
    return false;
}

bool EverQuic_uu___is_BRetry(EverQuic_long_header_specifics projectee)
{
  if (projectee.tag == EverQuic_BRetry)
    return true;
  else
    return false;
}

bool EverQuic_uu___is_BLong(EverQuic_header projectee)
{
  if (projectee.tag == EverQuic_BLong)
    return true;
  else
    return false;
}

bool EverQuic_uu___is_BShort(EverQuic_header projectee)
{
  if (projectee.tag == EverQuic_BShort)
    return true;
  else
    return false;
}

uint32_t EverQuic_dcid_len(EverQuic_header h)
{
  if (h.tag == EverQuic_BShort)
    return h.case_BShort.cid_len;
  else if (h.tag == EverQuic_BLong)
    return h.case_BLong.dcil;
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

bool EverQuic_is_retry(EverQuic_header h)
{
  if (EverQuic_uu___is_BLong(h))
  {
    EverQuic_long_header_specifics ite;
    if (h.tag == EverQuic_BLong)
      ite = h.case_BLong.spec;
    else
      ite =
        KRML_EABORT(EverQuic_long_header_specifics,
          "unreachable (pattern matches are exhaustive in F*)");
    return EverQuic_uu___is_BRetry(ite);
  }
  else
    return false;
}

uint32_t EverQuic_pn_length(EverQuic_header h)
{
  if (h.tag == EverQuic_BShort)
    return h.case_BShort.packet_number_length;
  else if (h.tag == EverQuic_BLong)
  {
    EverQuic_long_header_specifics spec = h.case_BLong.spec;
    if (spec.tag == EverQuic_BInitial)
      return spec.case_BInitial.packet_number_length;
    else if (spec.tag == EverQuic_BZeroRTT)
      return spec.case_BZeroRTT.packet_number_length;
    else if (spec.tag == EverQuic_BHandshake)
      return spec.case_BHandshake.packet_number_length;
    else
    {
      KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
        __FILE__,
        __LINE__,
        "unreachable (pattern matches are exhaustive in F*)");
      KRML_HOST_EXIT(255U);
    }
  }
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

bool EverQuic_has_payload_length(EverQuic_header h)
{
  if (EverQuic_uu___is_BLong(h))
  {
    EverQuic_long_header_specifics ite;
    if (h.tag == EverQuic_BLong)
      ite = h.case_BLong.spec;
    else
      ite =
        KRML_EABORT(EverQuic_long_header_specifics,
          "unreachable (pattern matches are exhaustive in F*)");
    return !EverQuic_uu___is_BRetry(ite);
  }
  else
    return false;
}

uint64_t EverQuic_payload_and_pn_length(EverQuic_header h)
{
  EverQuic_long_header_specifics scrut;
  if (h.tag == EverQuic_BLong)
    scrut = h.case_BLong.spec;
  else
    scrut =
      KRML_EABORT(EverQuic_long_header_specifics,
        "unreachable (pattern matches are exhaustive in F*)");
  if (scrut.tag == EverQuic_BInitial)
    return scrut.case_BInitial.payload_and_pn_length;
  else if (scrut.tag == EverQuic_BZeroRTT)
    return scrut.case_BZeroRTT.payload_and_pn_length;
  else if (scrut.tag == EverQuic_BHandshake)
    return scrut.case_BHandshake.payload_and_pn_length;
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

uint64_t EverQuic_payload_length(EverQuic_header h)
{
  return EverQuic_payload_and_pn_length(h) - (uint64_t)EverQuic_pn_length(h);
}

uint32_t EverQuic_public_header_len(EverQuic_header h)
{
  if (h.tag == EverQuic_BShort)
  {
    uint32_t cid_len = h.case_BShort.cid_len;
    return 1U + cid_len;
  }
  else if (h.tag == EverQuic_BLong)
  {
    EverQuic_long_header_specifics spec = h.case_BLong.spec;
    uint32_t scil = h.case_BLong.scil;
    uint32_t dcil = h.case_BLong.dcil;
    uint32_t ite;
    if (spec.tag == EverQuic_BInitial)
    {
      uint32_t token_length = spec.case_BInitial.token_length;
      uint64_t payload_and_pn_length1 = spec.case_BInitial.payload_and_pn_length;
      ite = varint_len((uint64_t)token_length) + token_length + varint_len(payload_and_pn_length1);
    }
    else if (spec.tag == EverQuic_BZeroRTT)
    {
      uint64_t payload_and_pn_length1 = spec.case_BZeroRTT.payload_and_pn_length;
      ite = varint_len(payload_and_pn_length1);
    }
    else if (spec.tag == EverQuic_BHandshake)
    {
      uint64_t payload_and_pn_length1 = spec.case_BHandshake.payload_and_pn_length;
      ite = varint_len(payload_and_pn_length1);
    }
    else if (spec.tag == EverQuic_BRetry)
    {
      uint32_t odcil = spec.case_BRetry.odcil;
      ite = 1U + odcil;
    }
    else
      ite = KRML_EABORT(uint32_t, "unreachable (pattern matches are exhaustive in F*)");
    return 7U + dcil + scil + ite;
  }
  else
  {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
      __FILE__,
      __LINE__,
      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

uint32_t EverQuic_header_len(EverQuic_header h)
{
  uint32_t ite;
  if (EverQuic_is_retry(h))
    ite = 0U;
  else
    ite = EverQuic_pn_length(h);
  return EverQuic_public_header_len(h) + ite;
}

