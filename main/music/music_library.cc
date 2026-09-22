#include "music_library.h"

#include <cJSON.h>

#include <algorithm>
#include <cstdint>

#include <sdkconfig.h>

#include "settings.h"

namespace {

constexpr const char* kSettingsNamespace = "music";
constexpr const char* kBaseUrlKey = "base_url";

// Well-known Vietnamese children's songs. Titles only; audio is supplied by
// the library base URL (see music_library.h).
const MusicTrack kTracks[] = {
    // Vietnamese (89)
    {"Ai thương con nhiều", "", "ai_thuong_con_nhieu.opus"},
    {"Ai thương con nhiều hơn", "", "ai_thuong_con_nhieu_hon.opus"},
    {"Alibaba", "", "alibaba.opus"},
    {"Ba bà đi bán lợn con", "ba ba di ban lon con", "ba_ba_di_ban_lon_con.opus"},
    {"Ba ngọn nến lung linh", "gia dinh nho", "ba_ngon_nen_lung_linh.opus"},
    {"Bàn tay xíu xíu", "", "ban_tay_xiu_xiu.opus"},
    {"Bé hát cho vui cửa vui nhà", "", "be_hat_cho_vui_cua_vui_nha.opus"},
    {"Bé khỏe bé ngoan", "", "be_khoe_be_ngoan.opus"},
    {"Bé quét nhà", "quet nha", "be_quet_nha.opus"},
    {"Bé Ti Ti", "be ti ti", "be_titi.opus"},
    {"Bông hồng tặng cô", "", "bong_hong_tang_co.opus"},
    {"Búp bê bằng bông", "be be bang bong;bup be", "bup_be_bang_bong.opus"},
    {"Bắc Kim Thang", "", "bac_kim_thang.opus"},
    {"Bố là tất cả", "", "bo_la_tat_ca.opus"},
    {"Chi chi chành chành", "", "chi_chi_chanh_chanh.opus"},
    {"Chiếc đèn ông sao", "den ong sao;ruoc den", "chiec_den_ong_sao.opus"},
    {"Cho con", "", "cho_con.opus"},
    {"Cháu lên ba", "", "chau_len_ba.opus"},
    {"Cháu yêu bà", "ba oi ba", "chau_yeu_ba.opus"},
    {"Chú Cuội", "chu cuoi choi trang", "chu_cuoi.opus"},
    {"Chú heo lười", "heo luoi", "chu_heo_luoi.opus"},
    {"Chú voi con ở Bản Đôn", "chu voi con;ban don", "chu_voi_con_o_ban_don.opus"},
    {"Chú vịt con", "", "chu_vit_con.opus"},
    {"Chú ếch con", "ech con;con ech", "chu_ech_con.opus"},
    {"Chị ong nâu và em bé", "chi ong nau", "chi_ong_nau_va_em_be.opus"},
    {"Chị thương em lắm", "", "chi_thuong_em_lam.opus"},
    {"Con chim non", "", "con_chim_non.opus"},
    {"Con cào cào", "cao cao", "con_cao_cao.opus"},
    {"Con cò bé bé", "con co", "con_co_be_be.opus"},
    {"Con gà trống", "ga trong", "con_ga_trong.opus"},
    {"Con heo đất", "heo dat", "con_heo_dat.opus"},
    {"Con ong cái kiến", "", "con_ong_cai_kien.opus"},
    {"Cả nhà thương nhau", "ba thuong con;gia dinh", "ca_nha_thuong_nhau.opus"},
    {"Dung dăng dung dẻ", "", "dung_dang_dung_de.opus"},
    {"Em là hoa hồng nhỏ", "hoa hong nho", "em_la_hoa_hong_nho.opus"},
    {"Em là mầm non của Đảng", "mam non cua dang", "em_la_mam_non_cua_dang.opus"},
    {"Em tập lái ô tô", "lai o to", "em_tap_lai_o_to.opus"},
    {"Em tập thể dục", "the duc buoi sang", "em_tap_the_duc.opus"},
    {"Em đi chơi thuyền", "di choi thuyen", "em_di_choi_thuyen.opus"},
    {"Gia đình nhỏ hạnh phúc to", "", "gia_dinh_nho_hanh_phuc_to.opus"},
    {"Gánh gánh gồng gồng", "", "ganh_ganh_gong_gong.opus"},
    {"Gặp mẹ trong mơ", "", "gap_me_trong_mo.opus"},
    {"Hoa bé ngoan", "", "hoa_be_ngoan.opus"},
    {"Hoa tay", "", "hoa_tay.opus"},
    {"Hoàng đế áo vải", "", "hoang_de_ao_vai.opus"},
    {"Họ nhà kỳ nhông", "", "ho_nha_ky_nhong.opus"},
    {"Khoảng trời mơ ước", "", "khoang_troi_mo_uoc.opus"},
    {"Kìa con bướm vàng", "con buom vang", "kia_con_buom_vang.opus"},
    {"Lý cây bông", "cay bong", "ly_cay_bong.opus"},
    {"Lời chào buổi sáng", "loi chao", "loi_chao_buoi_sang.opus"},
    {"Lời tre", "", "loi_tre.opus"},
    {"Mẹ ơi có biết", "", "me_oi_co_biet.opus"},
    {"Mẹ ơi Tết là gì", "", "me_oi_tet_la_gi.opus"},
    {"Một con vịt", "con vit", "mot_con_vit.opus"},
    {"Ngày Tết quê em", "ngay tet", "ngay_tet_que_em.opus"},
    {"Nhong nhong nhong", "", "nhong_nhong_nhong.opus"},
    {"Những cây bút chì màu", "but chi mau", "nhung_cay_but_chi_mau.opus"},
    {"Năm ngón tay ngoan", "", "nam_ngon_tay_ngoan.opus"},
    {"Nụ cười thiên thần", "", "nu_cuoi_thien_than.opus"},
    {"O tròn như trứng vịt", "chu o", "o_tron_nhu_trung_vit.opus"},
    {"Pháo hoa giao thừa", "", "phao_hoa_giao_thua.opus"},
    {"Rước đèn tháng tám", "ruoc den thang tam", "ruoc_den_thang_tam.opus"},
    {"Rửa mặt như mèo", "meo rua mat", "rua_mat_nhu_meo.opus"},
    {"Tay trắng tay đen", "", "tay_trang_tay_den.opus"},
    {"Thiếu nhi thế giới liên hoan", "", "thieu_nhi_the_gioi_lien_hoan.opus"},
    {"Thằng Cuội", "bong trang trang nga", "thang_cuoi.opus"},
    {"Tiếng hát bạn bè mình", "", "tieng_hat_ban_be_minh.opus"},
    {"Tiếng ve gọi hè", "", "tieng_ve_goi_he.opus"},
    {"Trong khu vườn", "", "trong_khu_vuon.opus"},
    {"Trăng tròn trăng trong", "", "trang_tron_trang_trong.opus"},
    {"Trường chúng cháu là trường mầm non", "truong mam non", "truong_chung_chau_la_truong_mam_non.opus"},
    {"Trời nắng trời mưa", "tho con", "troi_nang_troi_mua.opus"},
    {"Tập tầm vông", "", "tap_tam_vong.opus"},
    {"Tập đếm", "", "tap_dem.opus"},
    {"Tết ơi Tết", "", "tet_oi_tet.opus"},
    {"Từ rừng xanh cháu về thăm lăng Bác", "tu rung xanh", "tu_rung_xanh_chau_ve_tham_lang_bac.opus"},
    {"Vui Trung Thu", "trung thu", "vui_trung_thu.opus"},
    {"Vui đến trường", "", "vui_den_truong.opus"},
    {"Vòng tay ba mẹ", "", "vong_tay_ba_me.opus"},
    {"Vườn cây của ba", "vuon cay", "vuon_cay_cua_ba.opus"},
    {"Ánh trăng hòa bình", "anh trang", "anh_trang_hoa_binh.opus"},
    {"Đi học", "", "di_hoc.opus"},
    {"Đi học về", "", "di_hoc_ve.opus"},
    {"Đoàn tàu nhỏ xíu", "doan tau", "doan_tau_nho_xiu.opus"},
    {"Đàn gà con", "ga con", "dan_ga_con.opus"},
    {"Đàn kiến nó đi", "dan kien", "dan_kien_no_di.opus"},
    {"Đèn giao thông", "den do den xanh", "den_giao_thong.opus"},
    {"Đến trường thôi", "", "den_truong_thoi.opus"},
    {"Ước mơ hồng", "", "uoc_mo_hong.opus"},
    // English (22)
    {"ABC Song", "alphabet song;abcd", "abc_song.opus"},
    {"Are You Sleeping", "brother john;frere jacques", "are_you_sleeping.opus"},
    {"Baa Baa Black Sheep", "baa baa", "baa_baa_black_sheep.opus"},
    {"Baby Shark", "baby shark doo doo", "baby_shark.opus"},
    {"BINGO", "bingo was his name", "bingo.opus"},
    {"Finger Family", "daddy finger", "finger_family.opus"},
    {"Five Little Ducks", "five little ducks went out", "five_little_ducks.opus"},
    {"Five Little Monkeys", "five little monkeys jumping", "five_little_monkeys.opus"},
    {"Head, Shoulders, Knees and Toes", "head shoulders knees toes", "head_shoulders_knees_and_toes.opus"},
    {"Hickory Dickory Dock", "hickory dickory", "hickory_dickory_dock.opus"},
    {"Humpty Dumpty", "", "humpty_dumpty.opus"},
    {"If You're Happy and You Know It", "if youre happy;clap your hands", "if_you_re_happy_and_you_know_it.opus"},
    {"London Bridge Is Falling Down", "london bridge", "london_bridge_is_falling_down.opus"},
    {"Mary Had a Little Lamb", "mary had a lamb", "mary_had_a_little_lamb.opus"},
    {"Old MacDonald Had a Farm", "old macdonald;eieio", "old_macdonald_had_a_farm.opus"},
    {"Play Like a Dinosaur", "dinosaur", "play_like_a_dinosaur.opus"},
    {"Rain Rain Go Away", "rain rain", "rain_rain_go_away.opus"},
    {"Row, Row, Row Your Boat", "row your boat", "row_row_row_your_boat.opus"},
    {"Ten in the Bed", "roll over", "ten_in_the_bed.opus"},
    {"The Itsy Bitsy Spider", "itsy bitsy spider;incy wincy", "the_itsy_bitsy_spider.opus"},
    {"The Wheels on the Bus", "wheels on the bus", "the_wheels_on_the_bus.opus"},
    {"Twinkle Twinkle Little Star", "twinkle twinkle", "twinkle_twinkle_little_star.opus"},
};

constexpr size_t kTrackCount = sizeof(kTracks) / sizeof(kTracks[0]);

// Decode one UTF-8 sequence starting at index i; advances i past it.
uint32_t NextCodepoint(const std::string& s, size_t& i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
        i += 1;
        return c;
    }
    size_t extra = 0;
    uint32_t cp = 0;
    if ((c & 0xE0) == 0xC0) {
        extra = 1;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        extra = 2;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        extra = 3;
        cp = c & 0x07;
    } else {
        i += 1;  // Invalid lead byte; skip it.
        return 0xFFFD;
    }
    if (i + extra >= s.size()) {  // Truncated sequence at end of string.
        i = s.size();
        return 0xFFFD;
    }
    for (size_t k = 1; k <= extra; ++k) {
        unsigned char cc = static_cast<unsigned char>(s[i + k]);
        if ((cc & 0xC0) != 0x80) {
            i += 1;
            return 0xFFFD;
        }
        cp = (cp << 6) | (cc & 0x3F);
    }
    i += extra + 1;
    return cp;
}

// Map a Latin/Vietnamese codepoint to its unaccented ASCII letter.
char FoldCodepoint(uint32_t cp) {
    if (cp < 0x80) {
        char c = static_cast<char>(cp);
        if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
        return c;
    }
    // Latin-1 supplement
    if ((cp >= 0xC0 && cp <= 0xC5) || (cp >= 0xE0 && cp <= 0xE5)) return 'a';
    if ((cp >= 0xC8 && cp <= 0xCB) || (cp >= 0xE8 && cp <= 0xEB)) return 'e';
    if ((cp >= 0xCC && cp <= 0xCF) || (cp >= 0xEC && cp <= 0xEF)) return 'i';
    if ((cp >= 0xD2 && cp <= 0xD6) || (cp >= 0xF2 && cp <= 0xF6)) return 'o';
    if ((cp >= 0xD9 && cp <= 0xDC) || (cp >= 0xF9 && cp <= 0xFC)) return 'u';
    if (cp == 0xDD || cp == 0xFD || cp == 0xFF) return 'y';
    // Latin Extended-A / B letters used by Vietnamese
    if (cp == 0x102 || cp == 0x103) return 'a';  // Ă ă
    if (cp == 0x110 || cp == 0x111) return 'd';  // Đ đ
    if (cp == 0x128 || cp == 0x129) return 'i';  // Ĩ ĩ
    if (cp == 0x168 || cp == 0x169) return 'u';  // Ũ ũ
    if (cp == 0x1A0 || cp == 0x1A1) return 'o';  // Ơ ơ
    if (cp == 0x1AF || cp == 0x1B0) return 'u';  // Ư ư
    // Latin Extended Additional: Vietnamese tone marks, grouped by base letter.
    if (cp >= 0x1EA0 && cp <= 0x1EB7) return 'a';
    if (cp >= 0x1EB8 && cp <= 0x1EC7) return 'e';
    if (cp >= 0x1EC8 && cp <= 0x1ECB) return 'i';
    if (cp >= 0x1ECC && cp <= 0x1EE3) return 'o';
    if (cp >= 0x1EE4 && cp <= 0x1EF1) return 'u';
    if (cp >= 0x1EF2 && cp <= 0x1EF9) return 'y';
    return ' ';  // Anything else becomes a separator.
}

}  // namespace

namespace music {

std::string Normalize(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool last_space = true;  // Trims leading spaces.
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = NextCodepoint(text, i);
        char c = FoldCodepoint(cp);
        bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!alnum) {
            if (!last_space) {
                out.push_back(' ');
                last_space = true;
            }
            continue;
        }
        out.push_back(c);
        last_space = false;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

size_t Count() { return kTrackCount; }

const MusicTrack& At(size_t index) { return kTracks[index]; }

const MusicTrack* Find(const std::string& query) {
    std::string q = Normalize(query);
    if (q.empty()) return nullptr;

    const MusicTrack* partial = nullptr;
    for (size_t i = 0; i < kTrackCount; ++i) {
        const MusicTrack& t = kTracks[i];
        std::string title = Normalize(t.title);
        if (title == q) return &t;  // Exact title wins immediately.

        if (partial == nullptr &&
            (title.find(q) != std::string::npos || q.find(title) != std::string::npos)) {
            partial = &t;
        }

        // Aliases are ';' separated.
        std::string aliases = t.aliases ? t.aliases : "";
        size_t start = 0;
        while (start <= aliases.size() && !aliases.empty()) {
            size_t sep = aliases.find(';', start);
            std::string one =
                aliases.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
            std::string alias = Normalize(one);
            if (!alias.empty()) {
                if (alias == q) return &t;
                if (partial == nullptr && (alias.find(q) != std::string::npos ||
                                           q.find(alias) != std::string::npos)) {
                    partial = &t;
                }
            }
            if (sep == std::string::npos) break;
            start = sep + 1;
        }
    }
    return partial;
}

std::string GetBaseUrl() {
    // The build-time setting wins when it is set. Dictating a URL to a speech
    // recogniser is unreliable - ":8000" comes back as ":800" - and a mis-heard
    // value stored in NVS must not quietly override what was flashed. Leave
    // CONFIG_MUSIC_LIBRARY_URL empty to configure the library at runtime.
#ifdef CONFIG_MUSIC_LIBRARY_URL
    std::string configured = CONFIG_MUSIC_LIBRARY_URL;
    if (!configured.empty()) {
        return configured;
    }
#endif
    Settings settings(kSettingsNamespace);
    return settings.GetString(kBaseUrlKey);
}

void SetBaseUrl(const std::string& url) {
    Settings settings(kSettingsNamespace, true);
    settings.SetString(kBaseUrlKey, url);
}

std::string ResolveUrl(const MusicTrack& track) {
    std::string file = track.file ? track.file : "";
    if (file.empty()) return "";
    if (file.compare(0, 7, "http://") == 0 || file.compare(0, 8, "https://") == 0) {
        return file;
    }
    std::string base = GetBaseUrl();
    if (base.empty()) return "";
    if (base.back() == '/') base.pop_back();
    return base + "/" + file;
}

std::string ListJson() {
    bool have_base = !GetBaseUrl().empty();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "library_configured", have_base);
    cJSON* arr = cJSON_CreateArray();
    for (size_t i = 0; i < kTrackCount; ++i) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "title", kTracks[i].title);
        cJSON_AddBoolToObject(item, "playable", !ResolveUrl(kTracks[i]).empty());
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(root, "songs", arr);
    char* text = cJSON_PrintUnformatted(root);
    std::string out = text ? text : "{}";
    if (text) cJSON_free(text);
    cJSON_Delete(root);
    return out;
}

}  // namespace music
