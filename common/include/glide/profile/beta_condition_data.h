#pragma once
// Type=Condition models verified independently and identical in fourbetas. No modern identifiers substituted.
namespace glide {
namespace beta {
inline bool IsOwnCondition(unsigned build, unsigned buff) {
    if (build != 14075 && build != 14422 && build != 14505 && build != 14812)
        return false;
    switch (buff) {
    case 79:
    case 292:
    case 297:
    case 299:
    case 306:
    case 922:
    case 966:
    case 1012:
    case 1025:
    case 1728:
    case 7314:
        return true;
    default:
        return false;
    }
}
} // namespace beta
} // namespace glide
