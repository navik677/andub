#include "main_window.hpp"
#include "anime_card.hpp"
#include "details_view.hpp"
#include "downloads_view.hpp"
#include "../providers/anilibria_provider.hpp"
#include "../providers/animevost_provider.hpp"
#include "../services/favorites_manager.hpp"
#include "../services/theme_manager.hpp"
#include "../utils/str_utils.hpp"
#include <thread>
#include <vector>
#include <string>

namespace anime::ui {

static const std::vector<std::string> RUSSIAN_GENRES = {
    "Все жанры",
    "Боевые искусства",
    "Вампиры",
    "Гарем",
    "Демоны",
    "Детектив",
    "Дзёсей",
    "Драма",
    "Игры",
    "Исекай",
    "Исторический",
    "Киберпанк",
    "Комедия",
    "Магия",
    "Меха",
    "Мистика",
    "Музыка",
    "Пародия",
    "Повседневность",
    "Приключения",
    "Психологическое",
    "Романтика",
    "Сверхъестественное",
    "Сейнен",
    "Спорт",
    "Супер сила",
    "Сёдзе",
    "Сёдзе-ай",
    "Сёнен",
    "Триллер",
    "Ужасы",
    "Фантастика",
    "Фэнтези",
    "Школа",
    "Экшен",
    "Этти"
};

struct AppState {
    GtkWindow* window;
    GtkWidget* stack;
    GtkWidget* flow_box;
    GtkWidget* status_box;
    GtkWidget* spinner;
    GtkWidget* status_label;
    GtkWidget* search_entry;
    GtkWidget* genre_drop;
    GtkWidget* details_container;

    std::vector<std::shared_ptr<BaseProvider>> providers;
    size_t current_provider_idx = 0;
    bool is_favorites_mode = false;

    guint search_debounce_id = 0;
    std::string current_query;
    std::string current_genre;
};

struct SearchResultData {
    AppState* state;
    std::vector<Anime> items;
    std::string query;
    std::string genre;
};

static void do_search(AppState* state);

static gboolean on_search_results_ready(gpointer user_data) {
    auto* data = static_cast<SearchResultData*>(user_data);
    AppState* state = data->state;

    if (GTK_IS_SPINNER(state->spinner)) {
        gtk_spinner_stop(GTK_SPINNER(state->spinner));
        gtk_widget_set_visible(state->spinner, FALSE);
    }

    // Clear flow box
    GtkWidget* child = gtk_widget_get_first_child(state->flow_box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_flow_box_remove(GTK_FLOW_BOX(state->flow_box), child);
        child = next;
    }

    if (data->items.empty()) {
        gtk_widget_set_visible(state->status_box, TRUE);
        gtk_widget_set_visible(state->status_label, TRUE);
        std::string empty_msg = "Нічого не знайдено.";
        if (!data->genre.empty() && !data->query.empty()) {
            empty_msg = "Нічого не знайдено за запитом \"" + data->query + "\" та жанром \"" + data->genre + "\"";
        } else if (!data->genre.empty()) {
            empty_msg = "Нічого не знайдено за жанром: \"" + data->genre + "\"";
        }
        gtk_label_set_text(GTK_LABEL(state->status_label), empty_msg.c_str());
    } else {
        gtk_widget_set_visible(state->status_box, FALSE);
        gtk_widget_set_visible(state->status_label, FALSE);
        for (const auto& item : data->items) {
            GtkWidget* card = AnimeCard::create(item, [state](const Anime& anime) {
                // Open details view
                std::shared_ptr<BaseProvider> prov = state->providers[state->current_provider_idx];
                for (const auto& p : state->providers) {
                    if (p->name() == anime.provider) {
                        prov = p;
                        break;
                    }
                }

                // Remove previous details view child
                GtkWidget* old_child = gtk_widget_get_first_child(state->details_container);
                if (old_child) gtk_box_remove(GTK_BOX(state->details_container), old_child);

                GtkWidget* details = DetailsView::create(anime, prov, [state]() {
                    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "catalog");
                });
                gtk_box_append(GTK_BOX(state->details_container), details);
                gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "details");
            });

            gtk_flow_box_append(GTK_FLOW_BOX(state->flow_box), card);
            GtkWidget* child_widget = gtk_widget_get_parent(card);
            if (child_widget) {
                gtk_widget_set_halign(child_widget, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(child_widget, GTK_ALIGN_START);
                gtk_widget_set_focusable(child_widget, FALSE);
            }
        }
    }

    delete data;
    return G_SOURCE_REMOVE;
}

static void do_search(AppState* state) {
    if (state->status_box) {
        gtk_widget_set_visible(state->status_box, TRUE);
    }
    if (GTK_IS_SPINNER(state->spinner)) {
        gtk_widget_set_visible(state->spinner, TRUE);
        gtk_spinner_start(GTK_SPINNER(state->spinner));
    }
    gtk_widget_set_visible(state->status_label, FALSE);

    std::string q = state->current_query;
    std::string genre = state->current_genre;
    bool favs = state->is_favorites_mode;
    auto prov = state->providers[state->current_provider_idx];

    // Extract "жанр:..." or "genre:..." prefix if user typed it into search bar
    std::string query_text = q;
    std::string query_genre = genre;
    {
        std::string lower_q = utils::utf8_tolower(q);
        size_t pos = lower_q.find("жанр:");
        size_t pfx_len = 0;
        if (pos != std::string::npos) {
            // "жанр:" is 9 bytes in UTF-8
            pfx_len = std::string("жанр:").length();
        } else {
            pos = lower_q.find("genre:");
            if (pos != std::string::npos) pfx_len = 6;
        }

        if (pos != std::string::npos) {
            size_t val_start = pos + pfx_len;
            while (val_start < q.size() && (q[val_start] == ' ' || q[val_start] == '\t')) val_start++;
            size_t val_end = val_start;
            while (val_end < q.size() && q[val_end] != ' ' && q[val_end] != '\t') val_end++;
            if (val_end > val_start) {
                query_genre = q.substr(val_start, val_end - val_start);
                query_text = q.substr(0, pos) + q.substr(val_end);
                while (!query_text.empty() && std::isspace(static_cast<unsigned char>(query_text.front()))) query_text.erase(query_text.begin());
                while (!query_text.empty() && std::isspace(static_cast<unsigned char>(query_text.back()))) query_text.pop_back();
            }
        }
    }

    std::thread([state, query_text, query_genre, favs, prov]() {
        std::vector<Anime> results;
        if (favs) {
            auto all_favs = FavoritesManager::get_favorites();
            for (const auto& a : all_favs) {
                bool matches_query = query_text.empty() ||
                    utils::utf8_contains_ci(a.title_ru, query_text) ||
                    utils::utf8_contains_ci(a.title_en, query_text);

                bool matches_genre = query_genre.empty();
                if (!matches_genre) {
                    for (const auto& g : a.genres) {
                        if (utils::utf8_contains_ci(g, query_genre)) {
                            matches_genre = true;
                            break;
                        }
                    }
                }

                if (matches_query && matches_genre) {
                    results.push_back(a);
                }
            }
        } else {
            results = prov->search(query_text, 35, query_genre);
        }

        auto* res_data = new SearchResultData{state, std::move(results), query_text, query_genre};
        g_idle_add(on_search_results_ready, res_data);
    }).detach();
}

static gboolean on_debounce_timeout(gpointer user_data) {
    auto* state = static_cast<AppState*>(user_data);
    state->search_debounce_id = 0;
    do_search(state);
    return G_SOURCE_REMOVE;
}

GtkWidget* MainWindow::create(GtkApplication* app) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Anime GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 1080, 720);

    auto* state = new AppState();
    state->window = GTK_WINDOW(window);
    state->providers = {
        std::make_shared<AnilibriaProvider>(),
        std::make_shared<AnimeVostProvider>()
    };

    // HeaderBar
    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);

    // Left container: Provider switch, Russian Genre dropdown, Favorites button
    GtkWidget* left_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    const char* provider_names[] = {"АніЛібрія", "AnimeVost", nullptr};
    GtkWidget* prov_drop = gtk_drop_down_new_from_strings(provider_names);
    gtk_box_append(GTK_BOX(left_box), prov_drop);

    std::vector<const char*> genre_cstrs;
    for (const auto& g : RUSSIAN_GENRES) {
        genre_cstrs.push_back(g.c_str());
    }
    genre_cstrs.push_back(nullptr);
    GtkWidget* genre_drop = gtk_drop_down_new_from_strings(genre_cstrs.data());
    state->genre_drop = genre_drop;
    gtk_box_append(GTK_BOX(left_box), genre_drop);

    GtkWidget* fav_toggle = gtk_toggle_button_new_with_label("★ Улюблені");
    gtk_box_append(GTK_BOX(left_box), fav_toggle);

    g_signal_connect_data(
        prov_drop, "notify::selected",
        G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            s->current_provider_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
            if (!s->is_favorites_mode) {
                do_search(s);
            }
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    g_signal_connect_data(
        genre_drop, "notify::selected",
        G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
            if (sel == 0 || sel >= RUSSIAN_GENRES.size()) {
                s->current_genre.clear();
            } else {
                s->current_genre = RUSSIAN_GENRES[sel];
            }
            do_search(s);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    g_signal_connect_data(
        fav_toggle, "toggled",
        G_CALLBACK(+[](GtkToggleButton* btn, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            s->is_favorites_mode = gtk_toggle_button_get_active(btn);
            do_search(s);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), left_box);

    // Search Entry
    GtkWidget* search_entry = gtk_search_entry_new();
    gtk_widget_set_size_request(search_entry, 300, -1);
    gtk_search_entry_set_key_capture_widget(GTK_SEARCH_ENTRY(search_entry), window);
    state->search_entry = search_entry;
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), search_entry);

    g_signal_connect_data(
        search_entry, "search-changed",
        G_CALLBACK(+[](GtkSearchEntry* entry, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            s->current_query = gtk_editable_get_text(GTK_EDITABLE(entry));
            if (s->search_debounce_id > 0) {
                g_source_remove(s->search_debounce_id);
            }
            s->search_debounce_id = g_timeout_add(350, on_debounce_timeout, s);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    // Right container: Theme selector & Downloads button
    GtkWidget* right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    const auto& themes = ThemeManager::get_themes();
    std::vector<const char*> theme_labels;
    for (const auto& th : themes) {
        theme_labels.push_back(th.name.c_str());
    }
    theme_labels.push_back(nullptr);

    GtkWidget* theme_drop = gtk_drop_down_new_from_strings(theme_labels.data());
    std::string curr_th = ThemeManager::get_current_theme_id();
    for (guint i = 0; i < themes.size(); ++i) {
        if (themes[i].id == curr_th) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(theme_drop), i);
            break;
        }
    }

    g_signal_connect(
        theme_drop, "notify::selected",
        G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer) {
            guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
            const auto& ths = ThemeManager::get_themes();
            if (sel < ths.size()) {
                ThemeManager::apply_theme(ths[sel].id);
            }
        }),
        nullptr
    );
    gtk_box_append(GTK_BOX(right_box), theme_drop);

    GtkWidget* dl_btn = gtk_button_new_with_label("Завантаження");
    g_signal_connect_data(
        dl_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            DownloadsView::show_dialog(s->window);
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );
    gtk_box_append(GTK_BOX(right_box), dl_btn);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), right_box);

    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    // Main Stack
    GtkWidget* stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    state->stack = stack;

    // --- Catalog View ---
    GtkWidget* catalog_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Spinner and Status
    GtkWidget* status_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(status_box, 32);
    gtk_widget_set_margin_bottom(status_box, 16);
    state->status_box = status_box;

    GtkWidget* spinner = gtk_spinner_new();
    state->spinner = spinner;
    gtk_box_append(GTK_BOX(status_box), spinner);

    GtkWidget* status_lbl = gtk_label_new("");
    gtk_widget_add_css_class(status_lbl, "card-meta");
    state->status_label = status_lbl;
    gtk_box_append(GTK_BOX(status_box), status_lbl);

    gtk_box_append(GTK_BOX(catalog_box), status_box);

    // Scrolled window for Grid with generous bottom margin
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);

    GtkWidget* flow = gtk_flow_box_new();
    gtk_widget_set_valign(flow, GTK_ALIGN_START);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 2);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 12);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 20);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 20);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_widget_set_margin_start(flow, 24);
    gtk_widget_set_margin_end(flow, 24);
    gtk_widget_set_margin_top(flow, 20);
    gtk_widget_set_margin_bottom(flow, 80);
    state->flow_box = flow;

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), flow);
    gtk_box_append(GTK_BOX(catalog_box), scrolled);
    gtk_widget_set_margin_bottom(catalog_box, 32);

    gtk_stack_add_named(GTK_STACK(stack), catalog_box, "catalog");

    // --- Details Container ---
    GtkWidget* details_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->details_container = details_container;
    gtk_stack_add_named(GTK_STACK(stack), details_container, "details");

    gtk_window_set_child(GTK_WINDOW(window), stack);

    // Cleanup on destroy
    g_signal_connect_data(
        window, "destroy",
        G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
            auto* s = static_cast<AppState*>(user_data);
            if (s->search_debounce_id > 0) g_source_remove(s->search_debounce_id);
            delete s;
        }),
        state,
        nullptr,
        static_cast<GConnectFlags>(0)
    );

    // Initial search
    do_search(state);

    return window;
}

} // namespace anime::ui
