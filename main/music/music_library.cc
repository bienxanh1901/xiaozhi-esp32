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

// Fairy tales, played exactly like songs but from their own catalogue and the
// "stories/" folder of the library. Generated by music-server/gen_firmware.py.
const MusicTrack kStories[] = {
    // Vietnamese (491)
    {"Tấm Cám", "tam cam", "tam_cam.opus"},
    {"Sọ Dừa", "so dua", "so_dua.opus"},
    {"Thạch Sanh", "thach sanh;ly thong", "thach_sanh.opus"},
    {"Cây khế", "an khe tra vang", "cay_khe.opus"},
    {"Sự tích cây vú sữa", "cay vu sua", "su_tich_cay_vu_sua.opus"},
    {"Sự tích trầu cau", "trau cau", "su_tich_trau_cau.opus"},
    {"Sơn Tinh Thủy Tinh", "son tinh;thuy tinh", "son_tinh_thuy_tinh.opus"},
    {"Thánh Gióng", "giong", "thanh_giong.opus"},
    {"Con Rồng cháu Tiên", "lac long quan;au co", "con_rong_chau_tien.opus"},
    {"Bánh chưng bánh giầy", "banh chung;lang lieu", "banh_chung_banh_giay.opus"},
    {"Sự tích dưa hấu", "dua hau;mai an tiem", "su_tich_dua_hau.opus"},
    {"Cóc kiện Trời", "coc kien troi", "coc_kien_troi.opus"},
    {"Cậu bé thông minh", "cau be thong minh", "cau_be_thong_minh.opus"},
    {"Cô bé quàng khăn đỏ", "khan do", "co_be_quang_khan_do.opus"},
    {"Ba chú heo con", "ba chu heo", "ba_chu_heo_con.opus"},
    {"Bạch Tuyết và bảy chú lùn", "bach tuyet", "bach_tuyet_va_bay_chu_lun.opus"},
    {"Cô bé Lọ Lem", "lo lem", "co_be_lo_lem.opus"},
    {"Cô bé bán diêm", "ban diem", "co_be_ban_diem.opus"},
    {"Ông lão đánh cá và con cá vàng", "ong lao danh ca;con ca vang", "ong_lao_danh_ca_va_con_ca_vang.opus"},
    {"Nàng tiên cá", "tien ca", "nang_tien_ca.opus"},
    {"Ai mua hành tôi", "", "ai_mua_hanh_toi.opus"},
    {"Aladin và cây đèn thần", "", "aladin_va_cay_den_than.opus"},
    {"Anh chàng họ Đào", "", "anh_chang_ho_dao.opus"},
    {"Anh chàng thông minh", "", "anh_chang_thong_minh.opus"},
    {"Anh chồng ngốc", "", "anh_chong_ngoc.opus"},
    {"Ánh sáng và bóng đêm", "", "anh_sang_va_bong_dem.opus"},
    {"Anh thợ dệt nghèo", "", "anh_tho_det_ngheo.opus"},
    {"Anh thợ giày và ba tên trộm", "", "anh_tho_giay_va_ba_ten_trom.opus"},
    {"Anh và em gái", "", "anh_va_em_gai.opus"},
    {"Ba anh em ba nghề", "", "ba_anh_em_ba_nghe.opus"},
    {"Ba bó rơm nếp", "", "ba_bo_rom_nep.opus"},
    {"Ba cái chuồng trâu", "", "ba_cai_chuong_trau.opus"},
    {"Ba chàng dũng sĩ", "", "ba_chang_dung_si.opus"},
    {"Ba chàng hoàng tử", "", "ba_chang_hoang_tu.opus"},
    {"Ba chiếc lông chim", "", "ba_chiec_long_chim.opus"},
    {"Bà chúa bèo", "", "ba_chua_beo.opus"},
    {"Bà Chúa Trầm Hương", "", "ba_chua_tram_huong.opus"},
    {"Bà chúa Tuyết", "", "ba_chua_tuyet.opus"},
    {"Ba cô gái", "", "ba_co_gai.opus"},
    {"Bà già trong quả bầu", "", "ba_gia_trong_qua_bau.opus"},
    {"Ba hạt hồ đào", "", "ba_hat_ho_dao.opus"},
    {"Ba lá bùa", "", "ba_la_bua.opus"},
    {"Ba lưỡi rìu", "", "ba_luoi_riu.opus"},
    {"Ba người lùn trong rừng", "", "ba_nguoi_lun_trong_rung.opus"},
    {"Ba sợi tóc vàng của con quỷ", "", "ba_soi_toc_vang_cua_con_quy.opus"},
    {"Ba thiếu nữ tìm mặt trời", "", "ba_thieu_nu_tim_mat_troi.opus"},
    {"Bác làm vườn và nhà chủ", "", "bac_lam_vuon_va_nha_chu.opus"},
    {"Bác nông dân nghèo khó", "", "bac_nong_dan_ngheo_kho.opus"},
    {"Bạch Tuyết và Hồng Hoa", "bach tuyet va hoa hong", "bach_tuyet_va_hoa_hong.opus"},
    {"Bài hát của Okesa", "", "bai_hat_cua_okesa.opus"},
    {"Bán tóc đãi bạn", "", "ban_toc_dai_ban.opus"},
    {"Bảy anh em trai", "", "bay_anh_em_trai.opus"},
    {"Bảy cánh hoa vàng", "", "bay_canh_hoa_vang.opus"},
    {"Bầy chim thiên nga", "", "bay_chim_thien_nga.opus"},
    {"Bầy dê của hoàng đế", "", "bay_de_cua_hoang_de.opus"},
    {"Biển tiền vàng", "", "bien_tien_vang.opus"},
    {"Bộ da bà già", "", "bo_da_ba_gia.opus"},
    {"Bộ quần áo mới của hoàng đế", "", "bo_quan_ao_moi_cua_hoang_de.opus"},
    {"Bộ rẽ phép thuật của thần cây", "", "bo_re_phep_thuat_cua_than_cay.opus"},
    {"Bốn điều ước diệu kỳ", "", "bon_dieu_uoc_dieu_ky.opus"},
    {"Bụng làm dạ chịu", "", "bung_lam_da_chiu.opus"},
    {"Cá bống thần", "", "ca_bong_than.opus"},
    {"Cá cầu vồng kiêu ngạo", "", "ca_cau_vong_kieu_ngao.opus"},
    {"Cái ná chín rãnh", "", "cai_na_chin_ranh.opus"},
    {"Cái sừng của tên địa chủ", "", "cai_sung_cua_ten_dia_chu.opus"},
    {"Cái tai có phép lạ", "", "cai_tai_co_phep_la.opus"},
    {"Cassim và Alibaba", "", "cassim_va_alibaba.opus"},
    {"Cậu bé Binkie", "", "cau_be_binkie.opus"},
    {"Câu chuyện bốn mùa", "", "cau_chuyen_bon_mua.opus"},
    {"Cây bút thần", "", "cay_but_than.opus"},
    {"Cây củ cải", "", "cay_cu_cai.opus"},
    {"Cây nhãn, con dơi và con bướm", "", "cay_nhan_con_doi_va_con_buom.opus"},
    {"Cây tre trăm đốt", "", "cay_tre_tram_dot.opus"},
    {"Cây vĩ cầm có phép lạ", "", "cay_vi_cam_co_phep_la.opus"},
    {"Cha mẹ nuôi con bể hồ lai láng, con nuôi cha mẹ kể tháng kể ngày", "", "cha_me_nuoi_con_be_ho_lai_lang_con_nuoi_cha_me_ke_thang_ke_ngay.opus"},
    {"Chàng chăn cừu yêu tự do", "", "chang_chan_cuu_yeu_tu_do.opus"},
    {"Chàng chiến binh Ngón Tay Cái", "", "chang_chien_binh_ngon_tay_cai.opus"},
    {"Chàng Cơ-Ho và Công Chúa Nai Tơ-Lúy", "", "chang_co_ho_va_cong_chua_nai_to_luy.opus"},
    {"Chàng Cóc", "", "chang_coc.opus"},
    {"Chàng Damb’ri", "", "chang_damb_ri.opus"},
    {"Chàng đánh cá Mê Núp", "", "chang_danh_ca_me_nup.opus"},
    {"Chàng đốn củi và con tinh", "", "chang_don_cui_va_con_tinh.opus"},
    {"Chàng hiệp sĩ và người lái buôn", "", "chang_hiep_si_va_nguoi_lai_buon.opus"},
    {"Chàng hoàng tử và giấc mơ thiên đường", "", "chang_hoang_tu_va_giac_mo_thien_duong.opus"},
    {"Chàng Klang", "", "chang_klang.opus"},
    {"Chàng mồ côi", "", "chang_mo_coi.opus"},
    {"Chàng ngốc được kiện", "", "chang_ngoc_duoc_kien.opus"},
    {"Chàng ngốc học khôn", "", "chang_ngoc_hoc_khon.opus"},
    {"Chàng nông dân tinh khôn", "", "chang_nong_dan_tinh_khon.opus"},
    {"chàng rể hay chữ", "", "chang_re_hay_chu.opus"},
    {"Chàng Rêu thiện xạ và con rắn bạc", "", "chang_reu_thien_xa_va_con_ran_bac.opus"},
    {"Chàng Rùa", "", "chang_rua.opus"},
    {"Chàng Ta Thô dũng cảm", "", "chang_ta_tho_dung_cam.opus"},
    {"Chàng thợ săn Alek", "", "chang_tho_san_alek.opus"},
    {"Chàng Tom tham lam", "", "chang_tom_tham_lam.opus"},
    {"Chàng trai dũng cảm và quả lê vàng", "", "chang_trai_dung_cam_va_qua_le_vang.opus"},
    {"Chàng trai trái đào", "", "chang_trai_trai_dao.opus"},
    {"Chàng trai và đàn chim ri", "", "chang_trai_va_dan_chim_ri.opus"},
    {"Chàng Trăng", "", "chang_trang.opus"},
    {"Chiếc áo tàng hình", "", "chiec_ao_tang_hinh.opus"},
    {"Chiếc cầu phúc đức", "", "chiec_cau_phuc_duc.opus"},
    {"Chiếc hòm bay", "", "chiec_hom_bay.opus"},
    {"Chiếc hộp sắt biết nói", "", "chiec_hop_sat_biet_noi.opus"},
    {"Chiếc lắc vàng", "", "chiec_lac_vang.opus"},
    {"Chiếc nhẫn sắt có mặt đá quý", "", "chiec_nhan_sat_co_mat_da_quy.opus"},
    {"Chiếc rìu chỉ đường", "", "chiec_riu_chi_duong.opus"},
    {"Chiếc vương miện vàng của Vua Rắn", "", "chiec_vuong_mien_vang_cua_vua_ran.opus"},
    {"Chim Ruồi và Chim Diệc", "", "chim_ruoi_va_chim_diec.opus"},
    {"Chim sâu xử án", "", "chim_sau_xu_an.opus"},
    {"Chim Sẻ và Đại Bàng", "", "chim_se_va_dai_bang.opus"},
    {"Chim sơn ca của hoàng đế", "", "chim_son_ca_cua_hoang_de.opus"},
    {"Chó sói và bảy chú dê con", "", "cho_soi_va_bay_chu_de_con.opus"},
    {"Chó thần (Phần 1)", "", "cho_than_phan_1.opus"},
    {"Chó thần (Phần 2)", "", "cho_than_phan_2.opus"},
    {"Chồn con lười học", "", "chon_con_luoi_hoc.opus"},
    {"Chú bé chăn cừu và bọn cướp", "", "chu_be_chan_cuu_va_bon_cuop.opus"},
    {"Chú bé Jack và cây đậu thần", "", "chu_be_jack_va_cay_dau_than.opus"},
    {"Chú bé thành thật", "", "chu_be_thanh_that.opus"},
    {"Chú bé tí hon", "", "chu_be_ti_hon.opus"},
    {"Chú bé và ông trăng", "", "chu_be_va_ong_trang.opus"},
    {"Chử Đồng Tử và Tiên Dung", "", "chu_dong_tu_va_tien_dung.opus"},
    {"Chú gà trống mào vàng", "", "chu_ga_trong_mao_vang.opus"},
    {"Chú hổ biết nói tiếng người", "", "chu_ho_biet_noi_tieng_nguoi.opus"},
    {"Chú lính chì dũng cảm", "", "chu_linh_chi_dung_cam.opus"},
    {"Chú lùn Khúc Kha Khúc Khích", "", "chu_lun_khuc_kha_khuc_khich.opus"},
    {"Chú mèo đi hia", "", "chu_meo_di_hia.opus"},
    {"Chú mèo thông thái", "", "chu_meo_thong_thai.opus"},
    {"Chú thợ may nhỏ thó cam đảm", "", "chu_tho_may_nho_tho_cam_dam.opus"},
    {"Chưa đỗ ông nghè đã đe hàng tổng", "", "chua_do_ong_nghe_da_de_hang_tong.opus"},
    {"Chum vàng chum rắn", "", "chum_vang_chum_ran.opus"},
    {"Chuyện kể của vầng trăng", "ke cua vang trang", "chuyen_ke_cua_vang_trang.opus"},
    {"Chuyện những cây nến", "nhung cay nen", "chuyen_nhung_cay_nen.opus"},
    {"Cô bé hai mắt", "", "co_be_hai_mat.opus"},
    {"Cô bé quả lê", "", "co_be_qua_le.opus"},
    {"Cô bé tí hon", "", "co_be_ti_hon.opus"},
    {"Cô dâu đích thực", "", "co_dau_dich_thuc.opus"},
    {"Cô gái có mái tóc màu xanh", "", "co_gai_co_mai_toc_mau_xanh.opus"},
    {"Cô gái giẫm lên bánh mì", "", "co_gai_giam_len_banh_mi.opus"},
    {"Cô gái thông minh", "", "co_gai_thong_minh.opus"},
    {"Cố ghép", "", "co_ghep.opus"},
    {"Cỏ nến", "", "co_nen.opus"},
    {"Cổ tích giáng sinh", "", "co_tich_giang_sinh.opus"},
    {"Con cá vàng và nàng lãng quên", "", "con_ca_vang_va_nang_lang_quen.opus"},
    {"Con cáo đền ơn", "", "con_cao_den_on.opus"},
    {"Con chim bồ câu bạc", "", "con_chim_bo_cau_bac.opus"},
    {"Con chim khách màu nhiệm", "", "con_chim_khach_mau_nhiem.opus"},
    {"Con chim sơn ca", "", "con_chim_son_ca.opus"},
    {"Con chim vàng", "", "con_chim_vang.opus"},
    {"Con chó biết nói", "", "con_cho_biet_noi.opus"},
    {"Con chó, con mèo và anh chàng nghèo khổ", "", "con_cho_con_meo_va_anh_chang_ngheo_kho.opus"},
    {"Con chuột tinh ranh", "", "con_chuot_tinh_ranh.opus"},
    {"Con đường vượt hồ", "", "con_duong_vuot_ho.opus"},
    {"Con lạc đà trắng", "", "con_lac_da_trang.opus"},
    {"Con mèo đi giày cao gót", "", "con_meo_di_giay_cao_got.opus"},
    {"Con nai thần", "", "con_nai_than.opus"},
    {"Con ngỗng kỳ lạ", "", "con_ngong_ky_la.opus"},
    {"Con ngỗng vàng", "", "con_ngong_vang.opus"},
    {"Con quạ", "", "con_qua.opus"},
    {"Con quỷ trong chai thủy tinh", "", "con_quy_trong_chai_thuy_tinh.opus"},
    {"Con quỷ và ba người lính", "", "con_quy_va_ba_nguoi_linh.opus"},
    {"Con rắn trắng", "", "con_ran_trang.opus"},
    {"Con thiên nga bé bỏng", "", "con_thien_nga_be_bong.opus"},
    {"Con thiên nga đỏ thắm", "", "con_thien_nga_do_tham.opus"},
    {"Con yêu mẹ", "", "con_yeu_me.opus"},
    {"Công chúa chăn ngỗng", "", "cong_chua_chan_ngong.opus"},
    {"Công chúa hoa hồng", "", "cong_chua_hoa_hong.opus"},
    {"Công Chúa Tai Lừa", "", "cong_chua_tai_lua.opus"},
    {"Cọng rơm khô", "", "cong_rom_kho.opus"},
    {"Của thiên trả địa", "", "cua_thien_tra_dia.opus"},
    {"Cục bướu của ông già và lũ quỷ", "", "cuc_buou_cua_ong_gia_va_lu_quy.opus"},
    {"Cuộc phiêu lưu của bọ xấu xí", "", "cuoc_phieu_luu_cua_bo_xau_xi.opus"},
    {"Cuộc phiêu lưu của chàng Hạt Tiêu", "", "cuoc_phieu_luu_cua_chang_hat_tieu.opus"},
    {"Cuốn sách ma thuật", "", "cuon_sach_ma_thuat.opus"},
    {"Cứu vật, vật trả ơn. Cứu nhân, nhân báo oán", "", "cuu_vat_vat_tra_on_cuu_nhan_nhan_bao_oan.opus"},
    {"Đá dinh dưỡng", "", "da_dinh_duong.opus"},
    {"Đại bàng trả ơn", "", "dai_bang_tra_on.opus"},
    {"Dâu giỏi khỏi lo", "", "dau_gioi_khoi_lo.opus"},
    {"Dê đen và Dê trắng", "", "de_den_va_de_trang.opus"},
    {"Đêm giáng sinh cảm động", "", "dem_giang_sinh_cam_dong.opus"},
    {"Đẽo cày giữa đường", "", "deo_cay_giua_duong.opus"},
    {"Đi lấy mặt trời", "", "di_lay_mat_troi.opus"},
    {"Điều chưa từng có", "", "dieu_chua_tung_co.opus"},
    {"Diều Hâu và Gà Mái", "", "dieu_hau_va_ga_mai.opus"},
    {"Đoá hoa thần tiên", "", "doa_hoa_than_tien.opus"},
    {"Đôi giày bát-kết tự đi được", "", "doi_giay_bat_ket_tu_di_duoc.opus"},
    {"Đôi giày đỏ", "", "doi_giay_do.opus"},
    {"Đôi giày ủng da trâu và người lính già can đảm", "", "doi_giay_ung_da_trau_va_nguoi_linh_gia_can_dam.opus"},
    {"Đồng tiền Vạn Lịch", "", "dong_tien_van_lich.opus"},
    {"Đứa con vàng", "", "dua_con_vang.opus"},
    {"Dưa vàng đậu bạc", "", "dua_vang_dau_bac.opus"},
    {"Đường lên trời", "", "duong_len_troi.opus"},
    {"Ếch ăn trăng", "", "ech_an_trang.opus"},
    {"Em bé thông minh", "", "em_be_thong_minh.opus"},
    {"Ferdinand trung thực", "", "ferdinand_trung_thuc.opus"},
    {"Gà trống choai và mặt trời", "", "ga_trong_choai_va_mat_troi.opus"},
    {"Gấu bà", "", "gau_ba.opus"},
    {"Gấu lười đi mua dưa hấu", "", "gau_luoi_di_mua_dua_hau.opus"},
    {"Giận mày tao ở với ai", "", "gian_may_tao_o_voi_ai.opus"},
    {"Giăng bị thịt", "", "giang_bi_thit.opus"},
    {"Gieo hạt thần kỳ", "", "gieo_hat_than_ky.opus"},
    {"Gió và Mặt Trời", "", "gio_va_mat_troi.opus"},
    {"Gươm ông Tú", "", "guom_ong_tu.opus"},
    {"Hai anh em", "", "hai_anh_em.opus"},
    {"Hai anh em mồ côi", "", "hai_anh_em_mo_coi.opus"},
    {"Hai bảy mười ba", "", "hai_bay_muoi_ba.opus"},
    {"Hai chị em", "", "hai_chi_em.opus"},
    {"Hai vợ chồng và anh lái buôn", "", "hai_vo_chong_va_anh_lai_buon.opus"},
    {"Hansel và Gretel", "", "hansel_va_gretel.opus"},
    {"Hạt giống hoa mùa xuân", "", "hat_giong_hoa_mua_xuan.opus"},
    {"Hạt lựu của người Do Thái", "", "hat_luu_cua_nguoi_do_thai.opus"},
    {"Hồ cá vàng", "", "ho_ca_vang.opus"},
    {"Hổ, Sư Tử và Người", "", "ho_su_tu_va_nguoi.opus"},
    {"Hoa Khô", "", "hoa_kho.opus"},
    {"Hoàng thân keo kiệt", "", "hoang_than_keo_kiet.opus"},
    {"Hoàng tử chim", "", "hoang_tu_chim.opus"},
    {"Hoàng tử cua biển", "", "hoang_tu_cua_bien.opus"},
    {"Hoàng Tử Ếch", "", "hoang_tu_ech.opus"},
    {"Hoàng tử hạnh phúc", "", "hoang_tu_hanh_phuc.opus"},
    {"Hoàng tử thông minh", "", "hoang_tu_thong_minh.opus"},
    {"Hoàng tử trong rừng", "", "hoang_tu_trong_rung.opus"},
    {"Hoàng tử và tên nô bộc", "", "hoang_tu_va_ten_no_boc.opus"},
    {"Johannes trung thành", "", "johannes_trung_thanh.opus"},
    {"Jorinda và Joringel", "", "jorinda_va_joringel.opus"},
    {"Kéo cày trả nợ", "", "keo_cay_tra_no.opus"},
    {"Khăn Đỏ, Khăn Xanh", "", "khan_do_khan_xanh.opus"},
    {"Khi ông mặt trời nghỉ việc", "", "khi_ong_mat_troi_nghi_viec.opus"},
    {"Khu vườn ngọc trai", "", "khu_vuon_ngoc_trai.opus"},
    {"Lá bùa", "", "la_bua.opus"},
    {"Lấy chồng Dê", "", "lay_chong_de.opus"},
    {"Lời hứa của nhà vua", "", "loi_hua_cua_nha_vua.opus"},
    {"Lòng tham vô đáy", "", "long_tham_vo_day.opus"},
    {"Mèo chuột kết nghĩa", "", "meo_chuot_ket_nghia.opus"},
    {"Mẹo cổ thò", "", "meo_co_tho.opus"},
    {"Mẹo xử kiện", "", "meo_xu_kien.opus"},
    {"Mô-Mô-Ta-Rô, cậu bé quả đào", "", "mo_mo_ta_ro_cau_be_qua_dao.opus"},
    {"Món quà của các thiên thần", "", "mon_qua_cua_cac_thien_than.opus"},
    {"Món quà của cô giáo", "", "mon_qua_cua_co_giao.opus"},
    {"Món quà của rùa", "", "mon_qua_cua_rua.opus"},
    {"Món quà tặng mẹ", "", "mon_qua_tang_me.opus"},
    {"Một bà mẹ", "", "mot_ba_me.opus"},
    {"Một điều ước", "", "mot_dieu_uoc.opus"},
    {"Một đồng tiền vàng", "", "mot_dong_tien_vang.opus"},
    {"Mùa xuân ấm áp", "", "mua_xuan_am_ap.opus"},
    {"Mười hai hoàng tử", "", "muoi_hai_hoang_tu.opus"},
    {"Mười hai nàng công chúa và những đôi giày nhảy rách", "", "muoi_hai_nang_cong_chua_va_nhung_doi_giay_nhay_rach.opus"},
    {"Mưu trí bắt cướp", "", "muu_tri_bat_cuop.opus"},
    {"Nàng Bianat", "", "nang_bianat.opus"},
    {"Nàng công chúa bán than", "", "nang_cong_chua_ban_than.opus"},
    {"Nàng công chúa bị mất tích", "", "nang_cong_chua_bi_mat_tich.opus"},
    {"Nàng Công chúa hóa chim", "", "nang_cong_chua_hoa_chim.opus"},
    {"Nàng công chúa Kê-tơ", "", "nang_cong_chua_ke_to.opus"},
    {"Nàng công chúa kiêu căng", "", "nang_cong_chua_kieu_cang.opus"},
    {"Nàng công chúa lười", "", "nang_cong_chua_luoi.opus"},
    {"Nàng công chúa ngủ trong rừng", "", "nang_cong_chua_ngu_trong_rung.opus"},
    {"Nàng công chúa nhìn xa", "", "nang_cong_chua_nhin_xa.opus"},
    {"Nàng Giamita xinh đẹp", "", "nang_giamita_xinh_dep.opus"},
    {"Nàng Maleen", "", "nang_maleen.opus"},
    {"Nàng Ren Đắc hóa khỉ", "", "nang_ren_dac_hoa_khi.opus"},
    {"Nàng tiên biển", "", "nang_tien_bien.opus"},
    {"Nàng tiên thứ chín", "", "nang_tien_thu_chin.opus"},
    {"Nàng tiên trên thảo nguyên", "", "nang_tien_tren_thao_nguyen.opus"},
    {"Nàng tiên trong ngà voi", "", "nang_tien_trong_nga_voi.opus"},
    {"Nàng Út trong ống tre", "", "nang_ut_trong_ong_tre.opus"},
    {"Nàng Vaxilixa tốt bụng", "", "nang_vaxilixa_tot_bung.opus"},
    {"Nàng vỏ trứng", "", "nang_vo_trung.opus"},
    {"Nghe kể chuyện cổ tích thiếu nhi - Gà trống mượn tai", "", "nghe_ke_chuyen_co_tich_thieu_nhi_ga_trong_muon_tai.opus"},
    {"Nghe kể chuyện ngụ ngôn - Con Muỗi và Sư Tử", "", "nghe_ke_chuyen_ngu_ngon_con_muoi_va_su_tu.opus"},
    {"Nghe kể chuyện ngụ ngôn - Gà Rừng, Hoẵng và Rùa", "", "nghe_ke_chuyen_ngu_ngon_ga_rung_hoang_va_rua.opus"},
    {"Nghe kể chuyện ngụ ngôn - Những con voi và mặt trăng", "", "nghe_ke_chuyen_ngu_ngon_nhung_con_voi_va_mat_trang.opus"},
    {"Nghe kể chuyện ngụ ngôn - Ong mật và chim bồ câu", "", "nghe_ke_chuyen_ngu_ngon_ong_mat_va_chim_bo_cau.opus"},
    {"Nghe kể chuyện thiếu nhi - Ba giỏ khoai lang", "", "nghe_ke_chuyen_thieu_nhi_ba_gio_khoai_lang.opus"},
    {"Nghe kể chuyện thiếu nhi - Bà mặt trăng", "", "nghe_ke_chuyen_thieu_nhi_ba_mat_trang.opus"},
    {"Nghe kể chuyện thiếu nhi - Bác gấu đen và hai chú thỏ", "", "nghe_ke_chuyen_thieu_nhi_bac_gau_den_va_hai_chu_tho.opus"},
    {"Nghe kể chuyện thiếu nhi - Bí mật của mây nhỏ", "", "nghe_ke_chuyen_thieu_nhi_bi_mat_cua_may_nho.opus"},
    {"Nghe kể chuyện thiếu nhi - Biệt đội bảo vệ cây bằng lăng", "", "nghe_ke_chuyen_thieu_nhi_biet_doi_bao_ve_cay_bang_lang.opus"},
    {"Nghe kể chuyện thiếu nhi - Cá diếc con", "", "nghe_ke_chuyen_thieu_nhi_ca_diec_con.opus"},
    {"Nghe kể chuyện thiếu nhi - Cái hố bên đường", "", "nghe_ke_chuyen_thieu_nhi_cai_ho_ben_duong.opus"},
    {"Nghe kể chuyện thiếu nhi - Cái tết của mèo con", "", "nghe_ke_chuyen_thieu_nhi_cai_tet_cua_meo_con.opus"},
    {"Nghe kể chuyện thiếu nhi - Cảm ơn bạn Sóc", "", "nghe_ke_chuyen_thieu_nhi_cam_on_ban_soc.opus"},
    {"Nghe kể chuyện thiếu nhi - Câu chuyện trong vườn rau", "", "nghe_ke_chuyen_thieu_nhi_cau_chuyen_trong_vuon_rau.opus"},
    {"Nghe kể chuyện thiếu nhi - Cây gạo cõng mặt trời", "", "nghe_ke_chuyen_thieu_nhi_cay_gao_cong_mat_troi.opus"},
    {"Nghe kể chuyện thiếu nhi - Cây sồi mùa đông", "", "nghe_ke_chuyen_thieu_nhi_cay_soi_mua_dong.opus"},
    {"Nghe kể chuyện thiếu nhi - Cây táo kiêu kỳ", "", "nghe_ke_chuyen_thieu_nhi_cay_tao_kieu_ky.opus"},
    {"Nghe kể chuyện thiếu nhi - Chiếc găng tay", "", "nghe_ke_chuyen_thieu_nhi_chiec_gang_tay.opus"},
    {"Nghe kể chuyện thiếu nhi - Chiếc lá biết hát", "", "nghe_ke_chuyen_thieu_nhi_chiec_la_biet_hat.opus"},
    {"Nghe kể chuyện thiếu nhi - Chim di trú", "", "nghe_ke_chuyen_thieu_nhi_chim_di_tru.opus"},
    {"Nghe kể chuyện thiếu nhi - Chú đỗ con", "", "nghe_ke_chuyen_thieu_nhi_chu_do_con.opus"},
    {"Nghe kể chuyện thiếu nhi - Chú ếch du xuân", "", "nghe_ke_chuyen_thieu_nhi_chu_ech_du_xuan.opus"},
    {"Nghe kể chuyện thiếu nhi - Chú mèo ba chân", "", "nghe_ke_chuyen_thieu_nhi_chu_meo_ba_chan.opus"},
    {"Nghe kể chuyện thiếu nhi - Chú ngựa láu táu", "", "nghe_ke_chuyen_thieu_nhi_chu_ngua_lau_tau.opus"},
    {"Nghe kể chuyện thiếu nhi - Chuột nhắt du xuân", "", "nghe_ke_chuyen_thieu_nhi_chuot_nhat_du_xuan.opus"},
    {"Nghe kể chuyện thiếu nhi - Chuyện của thước kẻ", "", "nghe_ke_chuyen_thieu_nhi_chuyen_cua_thuoc_ke.opus"},
    {"Nghe kể chuyện thiếu nhi - Chuyện vui về chú ếch cốm", "", "nghe_ke_chuyen_thieu_nhi_chuyen_vui_ve_chu_ech_com.opus"},
    {"Nghe kể chuyện thiếu nhi - Cô ong nhỏ", "", "nghe_ke_chuyen_thieu_nhi_co_ong_nho.opus"},
    {"Nghe kể chuyện thiếu nhi - Cóc Tía và Sóc Bông Lau", "", "nghe_ke_chuyen_thieu_nhi_coc_tia_va_soc_bong_lau.opus"},
    {"Nghe kể chuyện thiếu nhi - Cuộc gặp gỡ thân thiện", "", "nghe_ke_chuyen_thieu_nhi_cuoc_gap_go_than_thien.opus"},
    {"Nghe kể chuyện thiếu nhi - Dê núi và nhền nhện", "", "nghe_ke_chuyen_thieu_nhi_de_nui_va_nhen_nhen.opus"},
    {"Nghe kể chuyện thiếu nhi - Điều kỳ diệu của mùa đông", "", "nghe_ke_chuyen_thieu_nhi_dieu_ky_dieu_cua_mua_dong.opus"},
    {"Nghe kể chuyện thiếu nhi - Đôi cánh tình bạn", "", "nghe_ke_chuyen_thieu_nhi_doi_canh_tinh_ban.opus"},
    {"Nghe kể chuyện thiếu nhi - Đời cây", "", "nghe_ke_chuyen_thieu_nhi_doi_cay.opus"},
    {"Nghe kể chuyện thiếu nhi - Đứa con của mẹ vịt bầu", "", "nghe_ke_chuyen_thieu_nhi_dua_con_cua_me_vit_bau.opus"},
    {"Nghe kể chuyện thiếu nhi - Ếch xanh đi học", "", "nghe_ke_chuyen_thieu_nhi_ech_xanh_di_hoc.opus"},
    {"Nghe kể chuyện thiếu nhi - Gấu Bông và Lợn Chút Chít", "", "nghe_ke_chuyen_thieu_nhi_gau_bong_va_lon_chut_chit.opus"},
    {"Nghe kể chuyện thiếu nhi - Giấc mơ của bé Phi", "", "nghe_ke_chuyen_thieu_nhi_giac_mo_cua_be_phi.opus"},
    {"Nghe kể chuyện thiếu nhi - Hoa lá và rễ", "", "nghe_ke_chuyen_thieu_nhi_hoa_la_va_re.opus"},
    {"Nghe kể chuyện thiếu nhi - Kẻ khoác lác", "", "nghe_ke_chuyen_thieu_nhi_ke_khoac_lac.opus"},
    {"Nghe kể chuyện thiếu nhi - Lời chào lá non", "", "nghe_ke_chuyen_thieu_nhi_loi_chao_la_non.opus"},
    {"Nghe kể chuyện thiếu nhi - Mèo hoa đi học", "", "nghe_ke_chuyen_thieu_nhi_meo_hoa_di_hoc.opus"},
    {"Nghe kể chuyện thiếu nhi - Mèo mun bị phạt", "", "nghe_ke_chuyen_thieu_nhi_meo_mun_bi_phat.opus"},
    {"Nghe kể chuyện thiếu nhi - Một ngày cho mẹ", "", "nghe_ke_chuyen_thieu_nhi_mot_ngay_cho_me.opus"},
    {"Nghe kể chuyện thiếu nhi - Mùa hè bình yên", "", "nghe_ke_chuyen_thieu_nhi_mua_he_binh_yen.opus"},
    {"Nghe kể chuyện thiếu nhi - Ngôi nhà của mẹ sẻ", "", "nghe_ke_chuyen_thieu_nhi_ngoi_nha_cua_me_se.opus"},
    {"Nghe kể chuyện thiếu nhi - Ngôi sao nhỏ đi tìm ánh sáng", "", "nghe_ke_chuyen_thieu_nhi_ngoi_sao_nho_di_tim_anh_sang.opus"},
    {"Nghe kể chuyện thiếu nhi - Người rộng lượng và kẻ keo kiệt", "", "nghe_ke_chuyen_thieu_nhi_nguoi_rong_luong_va_ke_keo_kiet.opus"},
    {"Nghe kể chuyện thiếu nhi - Ốc sên Si-pút", "", "nghe_ke_chuyen_thieu_nhi_oc_sen_si_put.opus"},
    {"Nghe kể chuyện thiếu nhi - Quà tặng của mặt trời", "", "nghe_ke_chuyen_thieu_nhi_qua_tang_cua_mat_troi.opus"},
    {"Nghe kể chuyện thiếu nhi - Quả táo của ai", "", "nghe_ke_chuyen_thieu_nhi_qua_tao_cua_ai.opus"},
    {"Nghe kể chuyện thiếu nhi - Quyển sách bị lãng quên", "", "nghe_ke_chuyen_thieu_nhi_quyen_sach_bi_lang_quen.opus"},
    {"Nghe kể chuyện thiếu nhi - Rùa cũng biết khóc", "", "nghe_ke_chuyen_thieu_nhi_rua_cung_biet_khoc.opus"},
    {"Nghe kể chuyện thiếu nhi - Số phận gã ruồi ong", "", "nghe_ke_chuyen_thieu_nhi_so_phan_ga_ruoi_ong.opus"},
    {"Nghe kể chuyện thiếu nhi - Tết bắt đầu từ bếp", "", "nghe_ke_chuyen_thieu_nhi_tet_bat_dau_tu_bep.opus"},
    {"Nghe kể chuyện thiếu nhi - Truyện kể của một đại văn hào", "", "nghe_ke_chuyen_thieu_nhi_truyen_ke_cua_mot_dai_van_hao.opus"},
    {"Nghe kể chuyện thiếu nhi - Truyện kể về rừng", "", "nghe_ke_chuyen_thieu_nhi_truyen_ke_ve_rung.opus"},
    {"Nghe kể chuyện thiếu nhi - Ước mơ của những hạt đậu", "", "nghe_ke_chuyen_thieu_nhi_uoc_mo_cua_nhung_hat_dau.opus"},
    {"Nghe kể chuyện thiếu nhi - Vì sao có mặt đất", "", "nghe_ke_chuyen_thieu_nhi_vi_sao_co_mat_dat.opus"},
    {"Nghe kể chuyện thiếu nhi - Voi mập và Chuột còi", "", "nghe_ke_chuyen_thieu_nhi_voi_map_va_chuot_coi.opus"},
    {"Nghe kể truyện cổ tích - Con rùa biết giật mình", "", "nghe_ke_truyen_co_tich_con_rua_biet_giat_minh.opus"},
    {"Nghe kể truyện cổ tích - Hoàng tử Ivan và con chim lửa", "", "nghe_ke_truyen_co_tich_hoang_tu_ivan_va_con_chim_lua.opus"},
    {"Ngôi nhà trong rừng", "", "ngoi_nha_trong_rung.opus"},
    {"Ngọn đèn xanh", "", "ngon_den_xanh.opus"},
    {"Ngọn đèn xanh và người lính dũng cảm", "", "ngon_den_xanh_va_nguoi_linh_dung_cam.opus"},
    {"Người cha đỡ đầu Thần Chết", "", "nguoi_cha_do_dau_than_chet.opus"},
    {"Người con hiếu thảo", "", "nguoi_con_hieu_thao.opus"},
    {"Người da gấu", "", "nguoi_da_gau.opus"},
    {"Người đàn ông mua chó, mèo và rắn", "", "nguoi_dan_ong_mua_cho_meo_va_ran.opus"},
    {"Người đào ếch thành võ trạng nguyên", "", "nguoi_dao_ech_thanh_vo_trang_nguyen.opus"},
    {"Người đi khắp thế gian", "", "nguoi_di_khap_the_gian.opus"},
    {"Người gieo hạt", "", "nguoi_gieo_hat.opus"},
    {"Người học trò và con chó đá", "", "nguoi_hoc_tro_va_con_cho_da.opus"},
    {"Người khách qua đường sáng suốt", "", "nguoi_khach_qua_duong_sang_suot.opus"},
    {"Người khổng lồ", "", "nguoi_khong_lo.opus"},
    {"Người lính xuống địa ngục", "", "nguoi_linh_xuong_dia_nguc.opus"},
    {"Người lùn dưới tảng đá", "", "nguoi_lun_duoi_tang_da.opus"},
    {"Người mẹ và thần chết", "", "nguoi_me_va_than_chet.opus"},
    {"Người mua giấc mơ", "", "nguoi_mua_giac_mo.opus"},
    {"Người nhạc sĩ lang thang", "", "nguoi_nhac_si_lang_thang.opus"},
    {"Người săn Linh Dương", "", "nguoi_san_linh_duong.opus"},
    {"Người thợ dệt thảm", "", "nguoi_tho_det_tham.opus"},
    {"Người thợ đúc", "", "nguoi_tho_duc.opus"},
    {"Người thợ mộc Nam Hoa", "", "nguoi_tho_moc_nam_hoa.opus"},
    {"Người thợ săn và con mụ chằn", "", "nguoi_tho_san_va_con_mu_chan.opus"},
    {"Người thợ xay bột và con mèo", "", "nguoi_tho_xay_bot_va_con_meo.opus"},
    {"Người thổi sáo và những chiếc xe", "", "nguoi_thoi_sao_va_nhung_chiec_xe.opus"},
    {"Người vợ tài đức", "", "nguoi_vo_tai_duc.opus"},
    {"Nguồn nước thần kỳ", "", "nguon_nuoc_than_ky.opus"},
    {"Ngưu Lang - Chức Nữ", "", "nguu_lang_chuc_nu.opus"},
    {"Nhà bay lên cung trăng", "", "nha_bay_len_cung_trang.opus"},
    {"Nhà Lừ đi tìm mẹ", "", "nha_lu_di_tim_me.opus"},
    {"Nhà vua và những hạt giống", "", "nha_vua_va_nhung_hat_giong.opus"},
    {"Nhà vua và ông già nghèo khổ", "", "nha_vua_va_ong_gia_ngheo_kho.opus"},
    {"Những chàng trai dũng cảm làng Kôn Men", "", "nhung_chang_trai_dung_cam_lang_kon_men.opus"},
    {"Những chữ cái anh em", "", "nhung_chu_cai_anh_em.opus"},
    {"Những cuộc phiêu lưu của chàng Sây", "", "nhung_cuoc_phieu_luu_cua_chang_say.opus"},
    {"Những tên ngốc", "", "nhung_ten_ngoc.opus"},
    {"Những tên trộm thua cuộc", "", "nhung_ten_trom_thua_cuoc.opus"},
    {"Nợ duyên trong mộng", "", "no_duyen_trong_mong.opus"},
    {"Nói dối như cuội", "", "noi_doi_nhu_cuoi.opus"},
    {"Nữ hoàng rắn Shahmaran", "", "nu_hoang_ran_shahmaran.opus"},
    {"Núi thủy tinh", "", "nui_thuy_tinh.opus"},
    {"Nước trường sinh", "", "nuoc_truong_sinh.opus"},
    {"Ơn bố mẹ", "", "on_bo_me.opus"},
    {"Ong chúa", "", "ong_chua.opus"},
    {"Ông già làm cây nở hoa", "", "ong_gia_lam_cay_no_hoa.opus"},
    {"Ông già làm gì cũng đúng", "", "ong_gia_lam_gi_cung_dung.opus"},
    {"Ông khổng lồ ích kỷ", "", "ong_khong_lo_ich_ky.opus"},
    {"Ống tiền hình con lợn", "", "ong_tien_hinh_con_lon.opus"},
    {"Ông vua ưa lời nói ngọt", "", "ong_vua_ua_loi_noi_ngot.opus"},
    {"Phép màu", "", "phep_mau.opus"},
    {"Pho mát tốt bụng", "", "pho_mat_tot_bung.opus"},
    {"Quả bầu tiên", "", "qua_bau_tien.opus"},
    {"Quả dưa dại", "", "qua_dua_dai.opus"},
    {"Quà tặng của đức vua", "", "qua_tang_cua_duc_vua.opus"},
    {"Quân tử", "", "quan_tu.opus"},
    {"Rapunzel Cô gái tóc dài", "", "rapunzel_co_gai_toc_dai.opus"},
    {"Sáu người kỳ tài", "", "sau_nguoi_ky_tai.opus"},
    {"Sợi dây nước mắt", "", "soi_day_nuoc_mat.opus"},
    {"Sói lại hoàn sói", "", "soi_lai_hoan_soi.opus"},
    {"Sợi râu hổ", "", "soi_rau_ho.opus"},
    {"Sói xám ngốc nghếch", "", "soi_xam_ngoc_nghech.opus"},
    {"Sứ giả mùa xuân", "", "su_gia_mua_xuan.opus"},
    {"Sự tích bãi ông Nam", "bai ong nam", "su_tich_bai_ong_nam.opus"},
    {"Sự tích bánh Trung Thu", "banh trung thu", "su_tich_banh_trung_thu.opus"},
    {"Sự tích bao lì xì ngày tết", "bao li xi ngay tet", "su_tich_bao_li_xi_ngay_tet.opus"},
    {"Sự tích bông hoa cúc", "bong hoa cuc", "su_tich_bong_hoa_cuc.opus"},
    {"Sự tích cây chổi", "cay choi", "su_tich_cay_choi.opus"},
    {"Sự tích cây dừa", "cay dua", "su_tich_cay_dua.opus"},
    {"Sự tích cây nêu ngày tết", "cay neu ngay tet", "su_tich_cay_neu_ngay_tet.opus"},
    {"Sự tích cây ngô", "cay ngo", "su_tich_cay_ngo.opus"},
    {"Sự tích cây nhãn", "cay nhan", "su_tich_cay_nhan.opus"},
    {"Sự tích cây nhân sâm", "cay nhan sam", "su_tich_cay_nhan_sam.opus"},
    {"Sự tích cây Tông Lông", "cay tong long", "su_tich_cay_tong_long.opus"},
    {"Sự tích chiếc kèn môi", "chiec ken moi", "su_tich_chiec_ken_moi.opus"},
    {"Sự tích chim chào mào đuôi đỏ", "chim chao mao duoi do", "su_tich_chim_chao_mao_duoi_do.opus"},
    {"Sự tích chim kền kền", "chim ken ken", "su_tich_chim_ken_ken.opus"},
    {"Sự tích chú Cuội ngồi gốc cây đa", "chu cuoi ngoi goc cay da", "su_tich_chu_cuoi_ngoi_goc_cay_da.opus"},
    {"Sự tích con cua", "con cua", "su_tich_con_cua.opus"},
    {"Sự tích con Dã Tràng", "con da trang", "su_tich_con_da_trang.opus"},
    {"Sự tích con Lợn", "con lon", "su_tich_con_lon.opus"},
    {"Sự tích con muỗi", "con muoi", "su_tich_con_muoi.opus"},
    {"Sự tích con tằm", "con tam", "su_tich_con_tam.opus"},
    {"Sự tích con Thạch Sùng", "con thach sung", "su_tich_con_thach_sung.opus"},
    {"Sự tích củ mài", "cu mai", "su_tich_cu_mai.opus"},
    {"Sự tích dưa bở", "dua bo", "su_tich_dua_bo.opus"},
    {"Sự tích hình thỏ mặt trăng", "hinh tho mat trang", "su_tich_hinh_tho_mat_trang.opus"},
    {"Sự tích hoa cẩm chướng", "hoa cam chuong", "su_tich_hoa_cam_chuong.opus"},
    {"Sự tích hoa cánh bướm", "hoa canh buom", "su_tich_hoa_canh_buom.opus"},
    {"Sự tích hoa cúc tây", "hoa cuc tay", "su_tich_hoa_cuc_tay.opus"},
    {"Sự tích hoa đào ngày tết", "hoa dao ngay tet", "su_tich_hoa_dao_ngay_tet.opus"},
    {"Sự tích hoa hồng", "hoa hong", "su_tich_hoa_hong.opus"},
    {"Sự tích hoa hồng vàng", "hoa hong vang", "su_tich_hoa_hong_vang.opus"},
    {"Sự tích hoa huệ", "hoa hue", "su_tich_hoa_hue.opus"},
    {"Sự tích hoa Huệ Tây", "hoa hue tay", "su_tich_hoa_hue_tay.opus"},
    {"Sự tích hoa mai vàng", "hoa mai vang", "su_tich_hoa_mai_vang.opus"},
    {"Sự tích hoa Nhài", "hoa nhai", "su_tich_hoa_nhai.opus"},
    {"Sự tích hoa phong lan", "hoa phong lan", "su_tich_hoa_phong_lan.opus"},
    {"Sự tích hoa phượng đỏ", "hoa phuong do", "su_tich_hoa_phuong_do.opus"},
    {"Sự tích hoa Thiên Lý", "hoa thien ly", "su_tich_hoa_thien_ly.opus"},
    {"Sự tích hoa Tử Đinh Hương Ba Tư", "hoa tu dinh huong ba tu", "su_tich_hoa_tu_dinh_huong_ba_tu.opus"},
    {"Sự tích Hoa Tuyết", "hoa tuyet", "su_tich_hoa_tuyet.opus"},
    {"Sự tích hòn đảo thuyền", "hon dao thuyen", "su_tich_hon_dao_thuyen.opus"},
    {"Sự tích hòn Vọng Phu", "hon vong phu", "su_tich_hon_vong_phu.opus"},
    {"Sự tích khăn tang", "khan tang", "su_tich_khan_tang.opus"},
    {"Sự tích loài ngỗng", "loai ngong", "su_tich_loai_ngong.opus"},
    {"Sự tích mùa xuân", "mua xuan", "su_tich_mua_xuan.opus"},
    {"Sự tích ngày Lễ Vu Lan báo hiếu", "ngay le vu lan bao hieu", "su_tich_ngay_le_vu_lan_bao_hieu.opus"},
    {"Sự tích người làm chúa muôn loài", "nguoi lam chua muon loai", "su_tich_nguoi_lam_chua_muon_loai.opus"},
    {"Sự tích người trong cung trăng", "nguoi trong cung trang", "su_tich_nguoi_trong_cung_trang.opus"},
    {"Sự Tích Núi Đàn Bà và Núi Đàn Ông", "nui dan ba va nui dan ong", "su_tich_nui_dan_ba_va_nui_dan_ong.opus"},
    {"Sự tích núi Mẫu Tử", "nui mau tu", "su_tich_nui_mau_tu.opus"},
    {"Sự tích núi Ngũ Hành Sơn", "nui ngu hanh son", "su_tich_nui_ngu_hanh_son.opus"},
    {"Sự tích nước biển mặn", "nuoc bien man", "su_tich_nuoc_bien_man.opus"},
    {"Sự tích ông Công ông Táo", "ong cong ong tao", "su_tich_ong_cong_ong_tao.opus"},
    {"Sự tích quả Loòng Boong", "qua loong boong", "su_tich_qua_loong_boong.opus"},
    {"Sự tích quả roi", "qua roi", "su_tich_qua_roi.opus"},
    {"Sự tích quốc đảo Singapore", "quoc dao singapore", "su_tich_quoc_dao_singapore.opus"},
    {"Sự tích sao Bắc Đẩu", "sao bac dau", "su_tich_sao_bac_dau.opus"},
    {"Sự tích sông Nhà Bè", "song nha be", "su_tich_song_nha_be.opus"},
    {"Sự tích thần núi Tản Viên", "than nui tan vien", "su_tich_than_nui_tan_vien.opus"},
    {"Sự tích thỏ tai dài đuôi ngắn", "tho tai dai duoi ngan", "su_tich_tho_tai_dai_duoi_ngan.opus"},
    {"Sự tích trái sầu riêng", "trai sau rieng", "su_tich_trai_sau_rieng.opus"},
    {"Sự tích vết trắng dưới cổ trâu", "vet trang duoi co trau", "su_tich_vet_trang_duoi_co_trau.opus"},
    {"Sư tử tuyết", "", "su_tu_tuyet.opus"},
    {"Tấm da bò", "", "tam_da_bo.opus"},
    {"Tàm hoa nương tử", "", "tam_hoa_nuong_tu.opus"},
    {"Tam và Tứ", "", "tam_va_tu.opus"},
    {"Tham ăn", "", "tham_an.opus"},
    {"Tham thì thâm", "", "tham_thi_tham.opus"},
    {"Thần biển và cậu bé mồi côi", "", "than_bien_va_cau_be_moi_coi.opus"},
    {"Thần gỗ", "", "than_go.opus"},
    {"Thần Mã", "", "than_ma.opus"},
    {"Thành Vangadu hồi sinh", "", "thanh_vangadu_hoi_sinh.opus"},
    {"Thiên lý mã hồng đào", "", "thien_ly_ma_hong_dao.opus"},
    {"Thỏ béo và ngôi nhà đặc biệt", "", "tho_beo_va_ngoi_nha_dac_biet.opus"},
    {"Thuỷ thần đền ơn", "", "thuy_than_den_on.opus"},
    {"Tiên hạc đền ơn", "", "tien_hac_den_on.opus"},
    {"Tiều A Lé", "", "tieu_a_le.opus"},
    {"Tìm mẹ", "", "tim_me.opus"},
    {"Trí khôn của ta đây", "", "tri_khon_cua_ta_day.opus"},
    {"Trọng nghĩa khinh tài", "", "trong_nghia_khinh_tai.opus"},
    {"Trường học phù thuỷ", "", "truong_hoc_phu_thuy.opus"},
    {"Truyện kể về cáo già xảo quyệt", "ke ve cao gia xao quyet", "truyen_ke_ve_cao_gia_xao_quyet.opus"},
    {"Truyền thuyết Hồ Tây", "", "truyen_thuyet_ho_tay.opus"},
    {"Từ Thức gặp tiên", "", "tu_thuc_gap_tien.opus"},
    {"Vàng lấy con vua", "", "vang_lay_con_vua.opus"},
    {"Ve Sầu đi học", "", "ve_sau_di_hoc.opus"},
    {"Vì sao cà phê nở hoa", "", "vi_sao_ca_phe_no_hoa.opus"},
    {"Vì sao hổ không ăn thịt mèo", "", "vi_sao_ho_khong_an_thit_meo.opus"},
    {"Vì sao lạc đà có bướu", "", "vi_sao_lac_da_co_buou.opus"},
    {"Vị thần của âm nhạc", "", "vi_than_cua_am_nhac.opus"},
    {"Vị thần của dân nghèo", "", "vi_than_cua_dan_ngheo.opus"},
    {"Viên ngọc ước của quạ", "", "vien_ngoc_uoc_cua_qua.opus"},
    {"Vụ kiện châu chấu", "", "vu_kien_chau_chau.opus"},
    {"Vua ếch", "", "vua_ech.opus"},
    {"Vua Nấm", "", "vua_nam.opus"},
    {"Vua Suleiman sáng suốt", "", "vua_suleiman_sang_suot.opus"},
    {"Vua Trộm", "", "vua_trom.opus"},
    {"Vui núi vàng", "", "vui_nui_vang.opus"},
    {"Vương Chất gặp tiên", "", "vuong_chat_gap_tien.opus"},
    {"Vương miện của rồng", "", "vuong_mien_cua_rong.opus"},
    {"Vương quốc hoa sen", "", "vuong_quoc_hoa_sen.opus"},
    {"Xin chôn ở núi vàng", "", "xin_chon_o_nui_vang.opus"},
    // English (18)
    {"Little Red Riding Hood", "red riding hood", "little_red_riding_hood.opus"},
    {"Hansel and Gretel", "hansel", "hansel_and_gretel.opus"},
    {"Cinderella", "", "cinderella.opus"},
    {"Snow White", "", "snow_white.opus"},
    {"The Princess and the Pea", "princess and the pea", "the_princess_and_the_pea.opus"},
    {"Aladdin and the magic lamp", "", "aladdin_and_the_magic_lamp.opus"},
    {"Ali Baba and the Forty Thieves", "", "ali_baba_and_the_forty_thieves.opus"},
    {"Little match girl", "", "little_match_girl.opus"},
    {"Rapunzel", "", "rapunzel.opus"},
    {"Six Swans", "", "six_swans.opus"},
    {"Sleeping Beauty", "", "sleeping_beauty.opus"},
    {"The Brave Little Tailor", "", "the_brave_little_tailor.opus"},
    {"The Emperor’s New Clothes", "", "the_emperor_s_new_clothes.opus"},
    {"The Fisherman and the Golden Fish", "", "the_fisherman_and_the_golden_fish.opus"},
    {"The Frog Prince", "", "the_frog_prince.opus"},
    {"The Golden Goose", "", "the_golden_goose.opus"},
    {"The Town Musicians of Bremen", "", "the_town_musicians_of_bremen.opus"},
    {"The White Snake", "", "the_white_snake.opus"},
};

constexpr size_t kStoryCount = sizeof(kStories) / sizeof(kStories[0]);

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

const MusicTrack* FindIn(const MusicTrack* table, size_t count, const std::string& query) {
    std::string q = Normalize(query);
    if (q.empty()) return nullptr;

    const MusicTrack* partial = nullptr;
    for (size_t i = 0; i < count; ++i) {
        const MusicTrack& t = table[i];
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

namespace {

std::string ResolveIn(const MusicTrack& track, const char* subdir) {
    std::string file = track.file ? track.file : "";
    if (file.empty()) return "";
    if (file.compare(0, 7, "http://") == 0 || file.compare(0, 8, "https://") == 0) {
        return file;
    }
    std::string base = GetBaseUrl();
    if (base.empty()) return "";
    if (base.back() == '/') base.pop_back();
    if (subdir != nullptr && subdir[0] != '\0') {
        base += "/";
        base += subdir;
    }
    return base + "/" + file;
}

// Lists at most `limit` entries, optionally only those matching `search`, so a
// large catalogue never turns into one huge allocation or MCP reply.
std::string ListJsonIn(const MusicTrack* table, size_t count, const char* subdir,
                       const char* key, const std::string& search, size_t limit) {
    bool have_base = !GetBaseUrl().empty();
    std::string q = Normalize(search);
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "library_configured", have_base);
    cJSON_AddNumberToObject(root, "total", static_cast<double>(count));
    cJSON* arr = cJSON_CreateArray();
    size_t matched = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!q.empty()) {
            std::string hay = Normalize(table[i].title);
            if (table[i].aliases && table[i].aliases[0]) {
                hay += " ";
                hay += Normalize(table[i].aliases);
            }
            if (hay.find(q) == std::string::npos) continue;
        }
        if (++matched > limit) continue;  // keep counting, stop adding
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "title", table[i].title);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddNumberToObject(root, "matched", static_cast<double>(matched));
    cJSON_AddBoolToObject(root, "truncated", matched > limit);
    cJSON_AddItemToObject(root, key, arr);
    char* text = cJSON_PrintUnformatted(root);
    std::string out = text ? text : "{}";
    if (text) cJSON_free(text);
    cJSON_Delete(root);
    return out;
}

}  // namespace

const MusicTrack* Find(const std::string& query) { return FindIn(kTracks, kTrackCount, query); }

std::string ResolveUrl(const MusicTrack& track) { return ResolveIn(track, ""); }

std::string ListJson() { return ListJsonIn(kTracks, kTrackCount, "", "songs", "", 200); }

}  // namespace music

namespace story {

size_t Count() { return kStoryCount; }

const MusicTrack* Find(const std::string& query) {
    return music::FindIn(kStories, kStoryCount, query);
}

std::string ResolveUrl(const MusicTrack& track) { return music::ResolveIn(track, "stories"); }

std::string ListJson(const std::string& search) {
    return music::ListJsonIn(kStories, kStoryCount, "stories", "stories", search, 30);
}

}  // namespace story
