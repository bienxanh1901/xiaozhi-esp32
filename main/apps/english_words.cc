#include "english_words.h"

#include <iterator>
#include <utility>

#include "material_symbols.h"

namespace english {

namespace {

constexpr Word kColors[] = {
    {"red", "màu đỏ", 0xE53935},       {"blue", "màu xanh dương", 0x1E88E5},
    {"green", "màu xanh lá", 0x43A047}, {"yellow", "màu vàng", 0xFDD835},
    {"orange", "màu cam", 0xFB8C00},    {"purple", "màu tím", 0x8E24AA},
    {"pink", "màu hồng", 0xF06292},     {"brown", "màu nâu", 0x795548},
    {"black", "màu đen", 0x000000},     {"white", "màu trắng", 0xFFFFFF},
    {"gray", "màu xám", 0x9E9E9E},
};

constexpr Word kNumbers[] = {
    {"one", "số một"},   {"two", "số hai"},   {"three", "số ba"},   {"four", "số bốn"},
    {"five", "số năm"},  {"six", "số sáu"},   {"seven", "số bảy"},  {"eight", "số tám"},
    {"nine", "số chín"}, {"ten", "số mười"},
};

constexpr Word kThings[] = {
    {"house", "ngôi nhà", 0, MATERIAL_SYMBOLS_HOME},
    {"phone", "điện thoại", 0, MATERIAL_SYMBOLS_PHONE},
    {"camera", "máy ảnh", 0, MATERIAL_SYMBOLS_PHOTO_CAMERA},
    {"star", "ngôi sao", 0, MATERIAL_SYMBOLS_STAR},
    {"heart", "trái tim", 0, MATERIAL_SYMBOLS_FAVORITE},
    {"key", "chìa khóa", 0, MATERIAL_SYMBOLS_KEY},
    {"lock", "ổ khóa", 0, MATERIAL_SYMBOLS_LOCK},
    {"watch", "đồng hồ đeo tay", 0, MATERIAL_SYMBOLS_WATCH},
    {"headphones", "tai nghe", 0, MATERIAL_SYMBOLS_HEADPHONES},
    {"glasses", "cái kính", 0, MATERIAL_SYMBOLS_EYEGLASSES},
    {"letter", "lá thư", 0, MATERIAL_SYMBOLS_MAIL},
    {"robot", "người máy", 0, MATERIAL_SYMBOLS_ROBOT_2},
    {"alarm clock", "đồng hồ báo thức", 0, MATERIAL_SYMBOLS_ALARM},
    {"calendar", "quyển lịch", 0, MATERIAL_SYMBOLS_CALENDAR_MONTH},
    {"bell", "cái chuông", 0, MATERIAL_SYMBOLS_NOTIFICATIONS},
};

constexpr Word kAnimals[] = {
    {"cat", "con mèo"},       {"dog", "con chó"},     {"bird", "con chim"},
    {"fish", "con cá"},       {"duck", "con vịt"},    {"chicken", "con gà"},
    {"pig", "con lợn"},       {"cow", "con bò"},      {"horse", "con ngựa"},
    {"monkey", "con khỉ"},    {"elephant", "con voi"}, {"tiger", "con hổ"},
    {"rabbit", "con thỏ"},    {"bear", "con gấu"},
};

constexpr Word kFruits[] = {
    {"apple", "quả táo"},      {"banana", "quả chuối"},   {"orange", "quả cam"},
    {"mango", "quả xoài"},     {"grapes", "quả nho"},     {"watermelon", "dưa hấu"},
    {"lemon", "quả chanh"},    {"pineapple", "quả dứa"},  {"coconut", "quả dừa"},
    {"strawberry", "dâu tây"}, {"peach", "quả đào"},
};

constexpr Word kBody[] = {
    {"face", "khuôn mặt"}, {"tongue", "cái lưỡi"},  {"eye", "con mắt"}, {"ear", "cái tai"},
    {"nose", "cái mũi"}, {"mouth", "cái miệng"}, {"teeth", "răng"}, {"hand", "bàn tay"},
    {"foot", "bàn chân"}, {"arm", "cánh tay"}, {"leg", "cái chân"},
};

constexpr Word kFood[] = {
    {"bread", "bánh mì"},    {"rice", "cơm"},         {"egg", "quả trứng"},
    {"milk", "sữa"},         {"cake", "bánh ngọt"},   {"candy", "cái kẹo"},
    {"ice cream", "kem"},    {"pizza", "bánh pizza"}, {"noodles", "mì sợi"},
    {"cheese", "phô mai"},   {"cookie", "bánh quy"},  {"juice", "nước ép"},
    {"water", "nước"},
};

constexpr Word kVegetables[] = {
    {"carrot", "cà rốt"},       {"corn", "bắp ngô"},      {"potato", "khoai tây"},
    {"tomato", "cà chua"},      {"cucumber", "dưa chuột"}, {"onion", "củ hành"},
    {"garlic", "củ tỏi"},       {"chili", "quả ớt"},      {"mushroom", "cây nấm"},
    {"broccoli", "súp lơ xanh"}, {"eggplant", "cà tím"},
};

constexpr Word kVehicles[] = {
    {"car", "ô tô"},          {"bus", "xe buýt"},       {"bicycle", "xe đạp"},
    {"motorbike", "xe máy"},  {"train", "tàu hỏa"},     {"airplane", "máy bay"},
    {"boat", "con thuyền"},   {"ship", "tàu thủy"},     {"truck", "xe tải"},
    {"taxi", "xe taxi"},      {"rocket", "tên lửa"},    {"helicopter", "trực thăng"},
};

constexpr Word kNature[] = {
    {"sun", "mặt trời"},    {"moon", "mặt trăng"}, {"cloud", "đám mây"},
    {"rain", "cơn mưa"},    {"snow", "tuyết"},     {"rainbow", "cầu vồng"},
    {"tree", "cái cây"},    {"flower", "bông hoa"}, {"leaf", "chiếc lá"},
    {"mountain", "ngọn núi"}, {"sea", "biển"},     {"fire", "ngọn lửa"},
};

constexpr Word kClothes[] = {
    {"shirt", "cái áo"},     {"dress", "váy đầm"},   {"pants", "cái quần"},
    {"hat", "cái mũ"},       {"cap", "mũ lưỡi trai"}, {"shoes", "đôi giày"},
    {"boots", "đôi ủng"},    {"socks", "đôi tất"},   {"coat", "áo khoác"},
    {"scarf", "khăn quàng"}, {"gloves", "găng tay"}, {"bag", "cái túi"},
};

constexpr Word kFamily[] = {
    {"father", "bố"},          {"mother", "mẹ"},    {"baby", "em bé"},
    {"boy", "cậu bé"},         {"girl", "cô bé"},   {"grandfather", "ông"},
    {"grandmother", "bà"},     {"family", "gia đình"},
};

constexpr Word kSchool[] = {
    {"book", "quyển sách"},   {"notebook", "quyển vở"}, {"pencil", "bút chì"},
    {"pen", "bút bi"},        {"crayon", "bút sáp"},    {"ruler", "thước kẻ"},
    {"scissors", "cái kéo"},  {"backpack", "cặp sách"}, {"paint", "màu vẽ"},
    {"computer", "máy tính"}, {"teacher", "cô giáo"},   {"school", "trường học"},
};

constexpr Word kToys[] = {
    {"ball", "quả bóng"},     {"kite", "con diều"},      {"teddy bear", "gấu bông"},
    {"balloon", "bóng bay"},  {"gift", "món quà"},       {"drum", "cái trống"},
    {"guitar", "đàn ghi ta"}, {"puzzle", "trò xếp hình"}, {"doll", "búp bê"},
};

constexpr Word kJobs[] = {
    {"doctor", "bác sĩ"},        {"farmer", "nông dân"},      {"cook", "đầu bếp"},
    {"police", "cảnh sát"},      {"firefighter", "lính cứu hỏa"}, {"pilot", "phi công"},
    {"singer", "ca sĩ"},         {"artist", "họa sĩ"},        {"astronaut", "phi hành gia"},
    {"builder", "thợ xây"},
};

constexpr Word kFeelings[] = {
    {"happy", "vui vẻ"},      {"sad", "buồn"},          {"angry", "tức giận"},
    {"scared", "sợ hãi"},     {"tired", "mệt mỏi"},     {"sleepy", "buồn ngủ"},
    {"surprised", "ngạc nhiên"}, {"sick", "bị ốm"},     {"cold", "lạnh"},
    {"hot", "nóng"},          {"love", "yêu thương"},   {"silly", "tinh nghịch"},
};

constexpr Word kHome[] = {
    {"bed", "cái giường"},    {"chair", "cái ghế"},     {"door", "cửa ra vào"},
    {"window", "cửa sổ"},     {"light bulb", "bóng đèn"}, {"toilet", "bồn cầu"},
    {"bathtub", "bồn tắm"},   {"sofa", "ghế sô pha"},   {"mirror", "cái gương"},
    {"clock", "đồng hồ"},     {"television", "ti vi"},  {"broom", "cái chổi"},
    {"soap", "xà phòng"},     {"toothbrush", "bàn chải đánh răng"},
};

constexpr Word kSports[] = {
    {"soccer", "bóng đá"},       {"basketball", "bóng rổ"},  {"tennis", "quần vợt"},
    {"swimming", "bơi lội"},     {"running", "chạy bộ"},     {"badminton", "cầu lông"},
    {"volleyball", "bóng chuyền"}, {"baseball", "bóng chày"}, {"cycling", "đạp xe"},
    {"skating", "trượt băng"},   {"boxing", "đấm bốc"},      {"golf", "đánh gôn"},
};

constexpr Word kSea[] = {
    {"whale", "cá voi"},     {"dolphin", "cá heo"},   {"shark", "cá mập"},
    {"octopus", "bạch tuộc"}, {"crab", "con cua"},    {"turtle", "con rùa"},
    {"jellyfish", "con sứa"}, {"shrimp", "con tôm"},  {"seal", "hải cẩu"},
    {"penguin", "chim cánh cụt"},
};

constexpr Word kInsects[] = {
    {"butterfly", "con bướm"}, {"bee", "con ong"},       {"ant", "con kiến"},
    {"ladybug", "bọ rùa"},     {"spider", "con nhện"},   {"snail", "ốc sên"},
    {"worm", "con giun"},      {"mosquito", "con muỗi"}, {"fly", "con ruồi"},
    {"beetle", "bọ cánh cứng"}, {"caterpillar", "sâu bướm"},
};

constexpr Word kWeather[] = {
    {"sun", "mặt trời"},     {"rain", "mưa"},        {"cloud", "mây"},
    {"snow", "tuyết"},       {"wind", "gió"},        {"storm", "bão"},
    {"rainbow", "cầu vồng"}, {"lightning", "tia chớp"}, {"fog", "sương mù"},
    {"umbrella", "cái ô"},
};

constexpr Word kShapes[] = {
    {"circle", "hình tròn"},     {"square", "hình vuông"}, {"triangle", "hình tam giác"},
    {"star", "hình ngôi sao"},  {"heart", "hình trái tim"}, {"diamond", "hình thoi"},
};

constexpr Word kActions[] = {
    {"eat", "ăn"},     {"drink", "uống"},   {"sleep", "ngủ"},     {"read", "đọc"},
    {"write", "viết"}, {"sing", "hát"},     {"dance", "nhảy múa"}, {"laugh", "cười"},
    {"cry", "khóc"},   {"wave", "vẫy tay"},
};

constexpr Word kInstruments[] = {
    {"guitar", "đàn ghi-ta"},   {"piano", "đàn piano"},        {"drum", "cái trống"},
    {"violin", "đàn vi-ô-lông"}, {"trumpet", "kèn trumpet"},   {"saxophone", "kèn saxophone"},
    {"accordion", "đàn phong cầm"}, {"banjo", "đàn banjo"},  {"bell", "cái chuông"},
};

constexpr Word kBirds[] = {
    {"bird", "con chim"},   {"owl", "con cú"},       {"eagle", "đại bàng"},
    {"parrot", "con vẹt"},  {"swan", "thiên nga"},   {"flamingo", "hồng hạc"},
    {"peacock", "con công"}, {"penguin", "chim cánh cụt"}, {"duck", "con vịt"},
    {"chicken", "con gà"},
};

constexpr Word kPlaces[] = {
    {"house", "ngôi nhà"},    {"school", "trường học"}, {"hospital", "bệnh viện"},
    {"shop", "cửa hàng"},     {"park", "công viên"},    {"beach", "bãi biển"},
    {"castle", "lâu đài"},    {"mountain", "núi"},
};

constexpr Word kSummer[] = {
    {"sun", "mặt trời"},    {"hot", "nóng"},           {"sea", "biển"},
    {"beach", "bãi biển"},  {"swimming", "bơi lội"},   {"ice cream", "kem"},
    {"juice", "nước ép"},   {"watermelon", "dưa hấu"}, {"crab", "con cua"},
    {"boat", "con thuyền"}, {"umbrella", "cái ô"},
};

constexpr Word kWinter[] = {
    {"snow", "tuyết"},         {"snowman", "người tuyết"},   {"cold", "lạnh"},
    {"coat", "áo khoác"},      {"scarf", "khăn quàng"},      {"gloves", "găng tay"},
    {"hat", "cái mũ"},         {"boots", "đôi ủng"},         {"socks", "đôi tất"},
    {"skating", "trượt băng"}, {"penguin", "chim cánh cụt"},
};

constexpr Word kZoo[] = {
    {"lion", "sư tử"},          {"tiger", "con hổ"},       {"elephant", "con voi"},
    {"giraffe", "hươu cao cổ"}, {"zebra", "ngựa vằn"},     {"monkey", "con khỉ"},
    {"panda", "gấu trúc"},      {"kangaroo", "chuột túi"}, {"crocodile", "cá sấu"},
    {"snake", "con rắn"},       {"bear", "con gấu"},
};

constexpr Word kSpace[] = {
    {"sun", "mặt trời"},
    {"moon", "mặt trăng"},
    {"star", "ngôi sao"},
    {"earth", "trái đất"},
    {"planet", "hành tinh"},
    {"rocket", "tên lửa"},
    {"astronaut", "phi hành gia"},
    {"alien", "người ngoài hành tinh"},
    {"telescope", "kính thiên văn"},
};

constexpr Topic kTopics[] = {
    {"Màu sắc", MATERIAL_SYMBOLS_BRIGHTNESS_6, 0xC62828, Picture::kColor, kColors,
     static_cast<int>(std::size(kColors))},
    {"Số đếm", MATERIAL_SYMBOLS_CALCULATE, 0x1565C0, Picture::kNumber, kNumbers,
     static_cast<int>(std::size(kNumbers))},
    {"Đồ vật", MATERIAL_SYMBOLS_KEY, 0x6A1B9A, Picture::kImage, kThings,
     static_cast<int>(std::size(kThings))},
    {"Con vật", MATERIAL_SYMBOLS_FAVORITE, 0x2E7D32, Picture::kImage, kAnimals,
     static_cast<int>(std::size(kAnimals))},
    {"Trái cây", MATERIAL_SYMBOLS_STAR, 0xEF6C00, Picture::kImage, kFruits,
     static_cast<int>(std::size(kFruits))},
    {"Cơ thể", MATERIAL_SYMBOLS_PERSON, 0x00838F, Picture::kImage, kBody,
     static_cast<int>(std::size(kBody))},
    {"Thức ăn", MATERIAL_SYMBOLS_FAVORITE, 0xD84315, Picture::kImage, kFood,
     static_cast<int>(std::size(kFood))},
    {"Rau củ", MATERIAL_SYMBOLS_STAR, 0x558B2F, Picture::kImage, kVegetables,
     static_cast<int>(std::size(kVegetables))},
    {"Xe cộ", MATERIAL_SYMBOLS_NAVIGATION, 0x283593, Picture::kImage, kVehicles,
     static_cast<int>(std::size(kVehicles))},
    {"Thiên nhiên", MATERIAL_SYMBOLS_EXPLORE, 0x0277BD, Picture::kImage, kNature,
     static_cast<int>(std::size(kNature))},
    {"Quần áo", MATERIAL_SYMBOLS_PERSON, 0xAD1457, Picture::kImage, kClothes,
     static_cast<int>(std::size(kClothes))},
    {"Gia đình", MATERIAL_SYMBOLS_HOME, 0x6D4C41, Picture::kImage, kFamily,
     static_cast<int>(std::size(kFamily))},
    {"Học tập", MATERIAL_SYMBOLS_EDIT_SQUARE, 0x4527A0, Picture::kImage, kSchool,
     static_cast<int>(std::size(kSchool))},
    {"Đồ chơi", MATERIAL_SYMBOLS_SPORTS_ESPORTS, 0x00695C, Picture::kImage, kToys,
     static_cast<int>(std::size(kToys))},
    {"Nghề nghiệp", MATERIAL_SYMBOLS_PERSON, 0x37474F, Picture::kImage, kJobs,
     static_cast<int>(std::size(kJobs))},
    {"Cảm xúc", MATERIAL_SYMBOLS_FAVORITE, 0xC2185B, Picture::kImage, kFeelings,
     static_cast<int>(std::size(kFeelings))},
    {"Trong nhà", MATERIAL_SYMBOLS_HOME, 0x8D6E63, Picture::kImage, kHome,
     static_cast<int>(std::size(kHome))},
    {"Thể thao", MATERIAL_SYMBOLS_SPORTS_ESPORTS, 0x2E7D32, Picture::kImage, kSports,
     static_cast<int>(std::size(kSports))},
    {"Biển cả", MATERIAL_SYMBOLS_EXPLORE, 0x01579B, Picture::kImage, kSea,
     static_cast<int>(std::size(kSea))},
    {"Côn trùng", MATERIAL_SYMBOLS_STAR, 0x9E9D24, Picture::kImage, kInsects,
     static_cast<int>(std::size(kInsects))},
    {"Thời tiết", MATERIAL_SYMBOLS_BRIGHTNESS_6, 0x0288D1, Picture::kImage, kWeather,
     static_cast<int>(std::size(kWeather))},
    {"Hình dạng", MATERIAL_SYMBOLS_STAR, 0x5E35B1, Picture::kImage, kShapes,
     static_cast<int>(std::size(kShapes))},
    {"Hành động", MATERIAL_SYMBOLS_PERSON, 0xE64A19, Picture::kImage, kActions,
     static_cast<int>(std::size(kActions))},
    {"Nhạc cụ", MATERIAL_SYMBOLS_MUSIC_NOTE, 0x8E24AA, Picture::kImage, kInstruments,
     static_cast<int>(std::size(kInstruments))},
    {"Các loài chim", MATERIAL_SYMBOLS_FAVORITE, 0x00796B, Picture::kImage, kBirds,
     static_cast<int>(std::size(kBirds))},
    {"Địa điểm", MATERIAL_SYMBOLS_LOCATION_ON, 0x455A64, Picture::kImage, kPlaces,
     static_cast<int>(std::size(kPlaces))},
    {"Mùa hè", MATERIAL_SYMBOLS_BRIGHTNESS_6, 0xF9A825, Picture::kImage, kSummer,
     static_cast<int>(std::size(kSummer))},
    {"Mùa đông", MATERIAL_SYMBOLS_DEVICE_THERMOSTAT, 0x1976D2, Picture::kImage, kWinter,
     static_cast<int>(std::size(kWinter))},
    {"Sở thú", MATERIAL_SYMBOLS_FAVORITE, 0x6D4C41, Picture::kImage, kZoo,
     static_cast<int>(std::size(kZoo))},
    {"Vũ trụ", MATERIAL_SYMBOLS_EXPLORE, 0x283593, Picture::kImage, kSpace,
     static_cast<int>(std::size(kSpace))},
};

}  // namespace

int TopicCount() { return static_cast<int>(std::size(kTopics)); }

const Topic& GetTopic(int index) { return kTopics[index]; }

void Quiz::Start(int word_count, Random random) {
    random_ = random;
    word_count_ = word_count < kChoices ? kChoices : word_count > kMaxWords ? kMaxWords : word_count;
    question_ = 0;
    score_ = 0;
    Shuffle();
    Ask();
}

void Quiz::Shuffle() {
    for (int i = 0; i < word_count_; ++i) order_[i] = i;
    for (int i = word_count_ - 1; i > 0; --i) {
        std::swap(order_[i], order_[random_() % (i + 1)]);
    }
    next_in_order_ = 0;
}

void Quiz::Ask() {
    if (next_in_order_ >= word_count_) {
        const int last = word_;
        Shuffle();
        // Do not ask the same word twice in a row across a reshuffle.
        if (order_[0] == last) std::swap(order_[0], order_[word_count_ - 1]);
    }
    word_ = order_[next_in_order_++];
    missed_ = false;
    answered_ = false;

    // The answer on a random button, other buttons different words.
    const int answer_at = random_() % kChoices;
    for (int i = 0; i < kChoices; ++i) {
        tried_[i] = false;
        if (i == answer_at) {
            choices_[i] = word_;
            continue;
        }
        int pick;
        bool taken;
        do {
            pick = random_() % word_count_;
            taken = pick == word_;
            for (int j = 0; j < i; ++j) taken = taken || choices_[j] == pick;
        } while (taken);
        choices_[i] = pick;
    }
}

Quiz::Result Quiz::Answer(int choice) {
    if (IsOver() || answered_ || choice < 0 || choice >= kChoices || tried_[choice]) {
        return Result::kIgnored;
    }
    tried_[choice] = true;
    if (choices_[choice] != word_) {
        missed_ = true;
        return Result::kWrong;
    }
    if (!missed_) ++score_;
    answered_ = true;
    return Result::kRight;
}

bool Quiz::Next() {
    if (IsOver() || !answered_) return false;
    ++question_;
    if (IsOver()) return false;
    Ask();
    return true;
}

}  // namespace english
