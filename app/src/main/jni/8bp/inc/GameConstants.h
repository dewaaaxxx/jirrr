#pragma once

// BALL PROPERTIES

inline constexpr int MAX_BALLS_COUNT = 16;
inline constexpr double BALL_RADIUS = 3.800475;
inline constexpr double BALL_RADIUS_SQUARE = BALL_RADIUS * BALL_RADIUS;



// TABLE SIZE CONSTANTS

inline constexpr double TABLE_WIDTH = 254.0;
inline constexpr double TABLE_HEIGHT = 127.0;

inline constexpr double TABLE_HALF_WIDTH = TABLE_WIDTH / 2.0;
inline constexpr double TABLE_HALF_HEIGHT = TABLE_HEIGHT / 2.0;

inline constexpr double TABLE_BOUND_LEFT = -TABLE_HALF_WIDTH + BALL_RADIUS;
inline constexpr double TABLE_BOUND_TOP = -TABLE_HALF_HEIGHT + BALL_RADIUS;
inline constexpr double TABLE_BOUND_RIGHT = TABLE_HALF_WIDTH - BALL_RADIUS;
inline constexpr double TABLE_BOUND_BOTTOM = TABLE_HALF_HEIGHT - BALL_RADIUS;



// OTHER TABLE PROPERTIES

inline constexpr int TABLE_POCKETS_COUNT = 6;
inline constexpr int TABLE_SHAPE_SIZE = 46;

inline constexpr double POCKET_RADIUS = 8.0;
inline constexpr double POCKET_RADIUS_SQUARE = POCKET_RADIUS * POCKET_RADIUS;

inline constexpr int MAX_SHOT_RESULT_SIZE = 50000;



// OTHER GAME PROPERTIES

inline constexpr double TIME_PER_TICK = 0.005;
inline constexpr double MIN_TIME = 1E-11;



// ANGLE CONSTANTS

inline constexpr double PI = 3.14159265358979;
inline constexpr double PI_0_5 = PI / 2.0;
inline constexpr double PI_1_5 = PI * 1.5;

inline constexpr double MAX_ANGLE_RADIANS = 360.0 / (180.0 / PI);
inline constexpr double MIN_ANGLE_STEP_RADIANS = 0.0174;

struct Candidate
{
    int    idx         = -1;   ///< Index of the target ball in SceneData::balls[]
    double angle       = 0.0;  ///< Shot angle in radians
    double score       = 1e18; ///< Lower is better (distance + penalties)
    int    pocketIndex = -1;   ///< Intended pocket (0–5)
    double power       = 0.0;  ///< Required shot power (internal units)
    double distTotal   = 0.0;  ///< Total path length (cue → ball → pocket)
    bool   isBandShot  = false;///< True for bank / combo shots
    double cutAngleCos = 0.0;  ///< cos(cut angle); higher = thinner cut

    bool operator<(const Candidate& o) const { return score < o.score; }
};
