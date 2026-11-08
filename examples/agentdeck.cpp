// agentdeck: GUI client for the pan-deck core (JSONL protocol v1 over a unix socket).
// Chat, tool log and agent/model controls; all protocol glue lives in DeckClient below.
//   agentdeck [--socket PATH] [--chat ID]
// Smoke hooks for headless runs: --auto-switch AGENT:MODEL, --auto-send TEXT, --auto-allow.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

#include <nlohmann/json.hpp>

#include "ChatView.hh"
#include "EventStream.hh"
#include "GuiBase.hh"
#include "LogPane.hh"
#include "imgui.h"

namespace {

using nlohmann::json;
using smg::ChatItem;
using smg::ChatMessage;
using Level = smg::LogPane::Level;

std::string str(const json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

bool flag(const json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_boolean() && it->get<bool>();
}

std::string dump(const json& j) { return j.dump(-1, ' ', false, json::error_handler_t::replace); }

// first line, cut to n bytes (not UTF-8 aware; the log is a glance, the chat has the full text)
std::string brief(const std::string& s, size_t n = 200) {
    std::string out = s.substr(0, s.find('\n'));
    if(out.size() > n) out = out.substr(0, n) + "...";
    return out;
}

// epoch seconds or ISO-8601 -> "HH:MM" today, else "Day HH:MM" (local time); raw text if unparsable
std::string when(const json& v) {
    std::time_t t = 0;
    if(v.is_number()) {
        t = static_cast<std::time_t>(v.get<double>());
    } else if(v.is_string()) {
        const std::string s = v.get<std::string>();
        int Y, M, D, h, m, sec;
        if(std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec) != 6) return s;
        std::tm tm{};
        tm.tm_year = Y - 1900;
        tm.tm_mon = M - 1;
        tm.tm_mday = D;
        tm.tm_hour = h;
        tm.tm_min = m;
        tm.tm_sec = sec;
        t = timegm(&tm);
        size_t p = s.find_first_of("Zz+-", 19); // after seconds: optional fraction, then the zone
        if(p != std::string::npos && s[p] != 'Z' && s[p] != 'z') {
            int oh = 0, om = 0;
            if(std::sscanf(s.c_str() + p + 1, "%d:%d", &oh, &om) >= 1) t -= (s[p] == '-' ? -1 : 1) * (oh * 3600 + om * 60);
        }
    } else {
        return {};
    }
    std::tm lt{}, now{};
    std::time_t n = std::time(nullptr);
    localtime_r(&t, &lt);
    localtime_r(&n, &now);
    const bool today = lt.tm_year == now.tm_year && lt.tm_yday == now.tm_yday;
    char buf[32];
    std::strftime(buf, sizeof buf, today ? "%H:%M" : "%a %H:%M", &lt);
    return buf;
}

int resize_cb(ImGuiInputTextCallbackData* d) {
    if(d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(d->UserData);
        str->resize(static_cast<size_t>(d->BufTextLen));
        d->Buf = str->data();
    }
    return 0;
}

bool input_text(const char* label, std::string& v, bool multiline) {
    const ImGuiInputTextFlags f = ImGuiInputTextFlags_CallbackResize;
    return multiline ? ImGui::InputTextMultiline(label, v.data(), v.capacity() + 1, ImVec2(0, 60), f, resize_cb, &v)
                     : ImGui::InputText(label, v.data(), v.capacity() + 1, f, resize_cb, &v);
}

struct OptionVal {
    std::string name, type, help;
    std::vector<std::string> choices;
    std::string s;
    bool b = false;
    int i = 0;
    double f = 0;
};

OptionVal option_from(const json& spec) {
    OptionVal o;
    o.name = str(spec, "name");
    o.type = str(spec, "type");
    o.help = str(spec, "help");
    auto ch = spec.find("choices");
    if(ch != spec.end() && ch->is_array())
        for(const auto& c : *ch)
            if(c.is_string()) o.choices.push_back(c.get<std::string>());
    auto d = spec.find("default");
    if(d != spec.end() && !d->is_null()) {
        if(d->is_boolean())
            o.b = d->get<bool>();
        else if(d->is_number_integer())
            o.i = d->get<int>(), o.f = o.i;
        else if(d->is_number())
            o.f = d->get<double>(), o.i = static_cast<int>(o.f);
        else if(d->is_string())
            o.s = d->get<std::string>();
    }
    if(o.type == "choice" && o.s.empty() && !o.choices.empty()) o.s = o.choices[0];
    return o;
}

json option_json(const OptionVal& o) {
    if(o.type == "bool") return o.b;
    if(o.type == "int") return o.i;
    if(o.type == "float") return o.f;
    return o.s; // str, text, choice
}

struct AgentInfo {
    std::string name;
    std::vector<json> schema;
};

struct ModelInfo {
    std::string id, provider, label, reason;
    bool available = true;
};

struct Smoke {
    std::string switch_to; // AGENT:MODEL
    std::string send;
    bool allow = false;
};

struct DeckClient {
    smg::EventStream es;
    smg::ChatView chat;
    smg::LogPane tools;
    Smoke smoke;

    // chat state (UI thread)
    std::string chat_id;
    bool running = false;
    std::string where = "core default"; // "agent / model" of the active chat
    std::optional<size_t> stream, group; // message taking text deltas / tool items
    std::map<std::string, std::pair<size_t, size_t>> calls; // call_id -> (message, item)
    json usage = json::object();
    std::string default_agent;
    std::string chat_arg; // --chat ID until it proves unknown

    // controls
    std::vector<AgentInfo> agents;
    std::vector<ModelInfo> models;
    std::string sel_agent, sel_model, want_model;
    std::vector<OptionVal> opts;
    bool sel_init = false, opened = false;
    bool live = false; // chat_opened seen on this connection
    bool send_pending = false; // a send awaiting its turn_started
    std::optional<size_t> last_user;
    json chat_opts = json::object(); // options of the chat's last switch
    std::string chat_agent; // agent from the chat's last switch, if any

    std::deque<json> perms;
    bool perm_open = false;

    // shared with the reader thread (on_connect); smoke flags are UI-thread only
    std::mutex mu;
    std::string want_chat;
    bool smoke_switched = false, smoke_sent = false;

    explicit DeckClient(const std::string& path, const std::string& chat_arg_) : es(path), chat_arg(chat_arg_), want_chat(chat_arg_) {
        es.on_connect = [this] {
            std::string c;
            {
                std::lock_guard<std::mutex> lk(mu);
                c = want_chat;
            }
            es.send({ { "type", "hello" }, { "version", 1 }, { "id", "hello" } });
            es.send({ { "type", "list_agents" }, { "id", "agents" } });
            es.send({ { "type", "list_models" }, { "id", "models0" } }); // no agent: learns the core default
            json open = { { "type", "open_chat" }, { "id", "open" } };
            if(c.empty())
                open["new"] = true;
            else
                open["chat_id"] = c;
            es.send(open);
        };
        es.start();
    }

    ~DeckClient() { es.stop(); } // joins the reader before mu/want_chat die

    bool ready() const { return live && es.state() == smg::EventStream::State::Connected && !chat_id.empty(); }

    bool send(json cmd, const char* id) {
        cmd["id"] = id;
        return es.send(cmd);
    }

    // -- chat model ------------------------------------------------------
    void note(const std::string& role, std::string text) {
        ChatMessage m;
        m.role = role;
        m.text = std::move(text);
        chat.model().add(std::move(m));
        stream = group = std::nullopt;
    }

    void divider(const std::string& agent, const std::string& model, bool lossy) {
        ChatMessage m;
        m.role = "system";
        m.divider = true;
        m.text = agent + " / " + model + (lossy ? " (lossy: history summarised)" : "");
        chat.model().add(std::move(m));
        stream = group = std::nullopt;
    }

    void text_delta(const std::string& t) {
        auto& cm = chat.model();
        if(!stream) {
            ChatMessage m;
            m.role = "assistant";
            stream = group = cm.add(std::move(m));
        }
        cm.append_text(*stream, t);
    }

    void tool_call(const json& e) {
        auto& cm = chat.model();
        if(!group) {
            ChatMessage m; // no role: avoids an empty "assistant" header
            group = cm.add(std::move(m));
        }
        stream = std::nullopt; // later text starts a new message after the tools
        ChatItem it;
        it.title = "tool: " + str(e, "name");
        it.body = e.contains("input") ? dump(e["input"]) : "";
        calls[str(e, "call_id")] = { *group, cm.add_item(*group, std::move(it)) };
    }

    void tool_result(const json& e) {
        auto it = calls.find(str(e, "call_id"));
        std::string out = str(e, "output") + (flag(e, "truncated") ? " [truncated]" : "");
        if(it == calls.end()) {
            note(flag(e, "is_error") ? "error" : "tool", out);
            return;
        }
        ChatItem& item = chat.model().item(it->second.first, it->second.second);
        item.body += "\n--\n" + out;
        item.error = flag(e, "is_error");
        if(item.error) item.open = true;
    }

    std::string prev_agent; // agent before the switch being rendered

    void render_entry(const json& e) {
        const std::string t = str(e, "type");
        if(t == "user")
            note("user", str(e, "text"));
        else if(t == "assistant") {
            ChatMessage m;
            m.role = "assistant";
            m.text = str(e, "text");
            group = chat.model().add(std::move(m));
            stream = std::nullopt;
        } else if(t == "tool_call")
            tool_call(e);
        else if(t == "tool_result")
            tool_result(e);
        else if(t == "switch") {
            const std::string a = str(e, "agent");
            // the core's rule: lossy when the agent changes and the target isn't native
            divider(a, str(e, "model"), !prev_agent.empty() && a != prev_agent && a != "native");
            prev_agent = a;
        }
    }

    // -- controls --------------------------------------------------------
    const AgentInfo* find_agent(const std::string& n) const {
        for(const auto& a : agents)
            if(a.name == n) return &a;
        return nullptr;
    }

    void select_agent(const std::string& name, const std::string& model, const json& o = json::object()) {
        sel_agent = name;
        want_model = model;
        sel_model.clear();
        models.clear();
        opts.clear();
        if(const AgentInfo* a = find_agent(name))
            for(const auto& s : a->schema) opts.push_back(option_from(s));
        for(auto& v : opts) {
            auto it = o.find(v.name);
            if(it == o.end()) continue;
            if(it->is_boolean())
                v.b = it->get<bool>();
            else if(it->is_number_integer())
                v.i = it->get<int>(), v.f = v.i;
            else if(it->is_number())
                v.f = it->get<double>(), v.i = static_cast<int>(v.f);
            else if(it->is_string())
                v.s = it->get<std::string>();
        }
        send({ { "type", "list_models" }, { "agent", name } }, "models");
    }

    void init_selection() {
        if(sel_init || !opened || agents.empty()) return;
        const std::string target = !chat_agent.empty() ? chat_agent : default_agent;
        if(target.empty()) return;
        sel_init = true;
        select_agent(find_agent(target) ? target : agents[0].name, want_model, chat_opts);
    }

    void apply() {
        json o = json::object();
        for(const auto& v : opts) o[v.name] = option_json(v);
        send({ { "type", "set_agent" }, { "chat_id", chat_id }, { "agent", sel_agent }, { "model", sel_model }, { "options", o } },
            "set_agent");
    }

    void submit(const std::string& text) {
        note("user", text);
        last_user = chat.model().messages().size() - 1;
        running = send_pending = true;
        send({ { "type", "send" }, { "chat_id", chat_id }, { "text", text } }, "send");
    }

    // -- events ----------------------------------------------------------
    void on_opened(const json& r) {
        chat_id = str(r, "chat_id");
        {
            std::lock_guard<std::mutex> lk(mu);
            want_chat = chat_id; // reconnects reopen this chat, not a new one
        }
        running = flag(r, "running");
        chat.model().clear();
        calls.clear();
        stream = group = std::nullopt;
        chat_agent.clear();
        want_model.clear();
        chat_opts = json::object();
        send_pending = false;
        last_user.reset();
        prev_agent = default_agent;
        std::string model;
        auto tr = r.find("transcript");
        if(tr != r.end() && tr->is_array())
            for(const auto& e : *tr) {
                render_entry(e);
                if(str(e, "type") == "switch") {
                    chat_agent = str(e, "agent"), model = str(e, "model");
                    auto o = e.find("options");
                    chat_opts = o != e.end() && o->is_object() ? *o : json::object();
                }
            }
        want_model = model;
        where =
            !chat_agent.empty() ? chat_agent + " / " + model : (default_agent.empty() ? "core default" : default_agent + " / core default");
        perms.clear();
        auto pp = r.find("pending_permissions");
        if(pp != r.end() && pp->is_array())
            for(const auto& p : *pp) perms.push_back(p);
        opened = live = true;
        sel_init = false;
        init_selection();
    }

    void on_models(const json& r) {
        const std::string agent = str(r, "agent");
        if(str(r, "id") == "models0") {
            default_agent = agent;
            if(chat_agent.empty() && opened && (where == "core default" || where.find(" / core default") != std::string::npos))
                where = agent + " / core default";
            init_selection();
            return;
        }
        if(agent != sel_agent) return; // stale answer for a combo the user already left
        models.clear();
        auto ms = r.find("models");
        if(ms != r.end() && ms->is_array())
            for(const auto& m : *ms) {
                ModelInfo mi;
                mi.id = str(m, "id");
                mi.provider = str(m, "provider");
                mi.label = str(m, "label");
                mi.reason = str(m, "reason");
                mi.available = !m.contains("available") || !m["available"].is_boolean() || m["available"].get<bool>();
                models.push_back(std::move(mi));
            }
        sel_model.clear();
        for(const auto& m : models)
            if(m.available && m.id == want_model) sel_model = m.id;
        if(sel_model.empty())
            for(const auto& m : models)
                if(m.available) {
                    sel_model = m.id;
                    break;
                }
    }

    void on_message(const json& m) {
        const std::string t = str(m, "type");
        if(t == "agents") {
            agents.clear();
            auto as = m.find("agents");
            if(as != m.end() && as->is_array())
                for(const auto& a : *as) {
                    AgentInfo ai;
                    ai.name = str(a, "name");
                    auto sc = a.find("options_schema");
                    if(sc != a.end() && sc->is_array()) ai.schema.assign(sc->begin(), sc->end());
                    agents.push_back(std::move(ai));
                }
            init_selection();
            return;
        }
        if(t == "models") return on_models(m);
        if(t == "chat_opened") return on_opened(m);
        if(t == "permission_request") {
            for(const auto& p : perms)
                if(str(p, "request_id") == str(m, "request_id")) return;
            perms.push_back(m);
            tools.add(Level::Info, "permission? " + str(m, "name") + " " + brief(m.contains("input") ? dump(m["input"]) : ""));
            if(smoke.allow) answer(true, false);
            return;
        }
        if(t == "permission_resolved") {
            const std::string rid = str(m, "request_id");
            perms.erase(
                std::remove_if(perms.begin(), perms.end(), [&](const json& p) { return str(p, "request_id") == rid; }), perms.end());
            tools.add(Level::Info, std::string("permission ") + (flag(m, "allow") ? "allowed" : "denied"));
            return;
        }
        if(t == "usage") { // account-wide: not chat-filtered
            for(const char* k : { "five_hour", "seven_day" })
                if(m.contains(k) && m[k].is_object()) usage[k] = m[k];
            return;
        }
        if(m.contains("id")) { // reply to one of our commands
            if(t == "error") {
                const std::string id = str(m, "id"), msg = str(m, "message");
                tools.add(Level::Error, id + ": " + msg);
                if(id == "perm") return; // late answer to a request someone else resolved
                if(id == "open" && !chat_arg.empty() && msg.rfind("unknown chat", 0) == 0) {
                    tools.add(Level::Warn, "unknown chat " + chat_arg + "; opening a new chat instead");
                    chat_arg.clear();
                    {
                        std::lock_guard<std::mutex> lk(mu);
                        want_chat.clear();
                    }
                    send({ { "type", "open_chat" }, { "new", true } }, "open");
                    return;
                }
                note("error", "error: " + msg);
                if(id == "send" && send_pending) { // rejected: no turn started
                    send_pending = running = false;
                    if(last_user) chat.model().append_text(*last_user, "  [not sent]");
                }
            }
            return;
        }
        const std::string cid = str(m, "chat_id");
        if(cid.empty() || cid != chat_id) return;
        if(t == "text_delta")
            text_delta(str(m, "text"));
        else if(t == "tool_call") {
            tool_call(m);
            tools.add(Level::Tool, "call " + str(m, "name") + " " + brief(m.contains("input") ? dump(m["input"]) : ""));
        } else if(t == "tool_result") {
            tool_result(m);
            const bool err = flag(m, "is_error");
            tools.add(err ? Level::Error : Level::Tool, std::string(err ? "error " : "result ") + brief(str(m, "output")));
        } else if(t == "turn_started") {
            running = true;
            send_pending = false;
            stream = group = std::nullopt;
        } else if(t == "turn_finished") {
            running = false;
            perms.erase(std::remove_if(perms.begin(), perms.end(), [&](const json& p) { return str(p, "chat_id") == cid; }), perms.end());
            stream = group = std::nullopt;
            if(str(m, "stop_reason") == "cancelled") note("system", "(cancelled)");
        } else if(t == "agent_switched") {
            divider(str(m, "agent"), str(m, "model"), flag(m, "lossy"));
            where = str(m, "agent") + " / " + str(m, "model");
            chat_agent = str(m, "agent");
            prev_agent = chat_agent;
            if(find_agent(chat_agent) && chat_agent != sel_agent) {
                select_agent(chat_agent, str(m, "model")); // controls follow the chat
            } else if(chat_agent == sel_agent) {
                want_model = str(m, "model");
                for(const auto& mi : models)
                    if(mi.available && mi.id == want_model) sel_model = want_model;
                if(m.contains("options") && m["options"].is_object()) select_agent(chat_agent, want_model, m["options"]);
            }
        } else if(t == "lossy") {
            note("system", "(history passed as summary)");
        } else if(t == "error") {
            note("error", "error: " + str(m, "message"));
            tools.add(Level::Error, str(m, "message"));
        }
    }

    void answer(bool allow, bool remember) {
        if(perms.empty()) return;
        const std::string rid = str(perms.front(), "request_id");
        const std::string what = allow ? (remember ? "allow-for-chat" : "allow") : "deny";
        if(!send({ { "type", "permission_response" }, { "request_id", rid }, { "allow", allow }, { "remember_for_chat", remember } },
               "perm")) {
            tools.add(Level::Error, "permission " + what + " not sent (core unreachable)");
            return;
        }
        tools.add(Level::Info, "permission " + what + " sent");
        perms.pop_front(); // the core's permission_resolved confirms; a stale answer just gets an error reply
    }

    // -- smoke hooks -------------------------------------------------------
    void drive_smoke() {
        if(!ready()) return;
        if(!smoke.switch_to.empty() && !smoke_switched) {
            smoke_switched = true;
            const auto c = smoke.switch_to.find(':');
            send({ { "type", "set_agent" },
                     { "chat_id", chat_id },
                     { "agent", smoke.switch_to.substr(0, c) },
                     { "model", c == std::string::npos ? "" : smoke.switch_to.substr(c + 1) },
                     { "options", json::object() } },
                "set_agent");
        }
        const bool switched = smoke.switch_to.empty() || where.rfind(smoke.switch_to.substr(0, smoke.switch_to.find(':')), 0) == 0;
        if(!smoke.send.empty() && !smoke_sent && switched && !running) {
            smoke_sent = true;
            submit(smoke.send);
        }
    }

    // -- drawing -----------------------------------------------------------
    std::string status_line() const {
        std::string s = where + (running ? " | running" : " | idle");
        const std::pair<const char*, const char*> windows[] = { { "five_hour", "5h" }, { "seven_day", "7d" } };
        for(const auto& [k, label] : windows) {
            if(!usage.contains(k)) continue;
            const json& w = usage[k];
            char buf[64] = "";
            if(w.contains("pct") && w["pct"].is_number())
                std::snprintf(buf, sizeof buf, "%s %g%%", label, w["pct"].get<double>());
            else
                std::snprintf(buf, sizeof buf, "%s ?", label);
            s += std::string(" | ") + buf;
            if(w.contains("resets_at") && !w["resets_at"].is_null()) s += " (resets " + when(w["resets_at"]) + ")";
        }
        return s;
    }

    void draw_chat() {
        const bool up = es.state() == smg::EventStream::State::Connected;
        ImGui::TextUnformatted(status_line().c_str());
        if(!up) {
            ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f), "core not running - start with: pan-deck serve");
            ImGui::TextDisabled("%s", es.status().c_str());
        }
        ImGui::Separator();
        if(auto text = chat.draw(ready() && !running)) submit(*text);
    }

    void draw_controls() {
        if(ImGui::BeginCombo("agent", sel_agent.c_str())) {
            std::string pick;
            for(const auto& a : agents)
                if(ImGui::Selectable(a.name.c_str(), a.name == sel_agent) && a.name != sel_agent) pick = a.name;
            ImGui::EndCombo();
            if(!pick.empty()) select_agent(pick, "");
        }
        std::string cur;
        for(const auto& m : models)
            if(m.id == sel_model) cur = m.label.empty() ? m.id : m.label;
        if(ImGui::BeginCombo("model", cur.c_str())) {
            for(const auto& m : models) {
                const std::string label = (m.label.empty() ? m.id : m.label) + (m.provider.empty() ? "" : "  [" + m.provider + "]");
                if(ImGui::Selectable((label + "##" + m.id).c_str(), m.id == sel_model, m.available ? 0 : ImGuiSelectableFlags_Disabled))
                    sel_model = m.id;
                if(!m.available && !m.reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("%s", m.reason.c_str());
            }
            ImGui::EndCombo();
        }
        for(auto& o : opts) {
            ImGui::PushID(o.name.c_str());
            if(o.type == "bool")
                ImGui::Checkbox(o.name.c_str(), &o.b);
            else if(o.type == "int")
                ImGui::InputInt(o.name.c_str(), &o.i);
            else if(o.type == "float")
                ImGui::InputDouble(o.name.c_str(), &o.f);
            else if(o.type == "choice") {
                if(ImGui::BeginCombo(o.name.c_str(), o.s.c_str())) {
                    for(const auto& c : o.choices)
                        if(ImGui::Selectable(c.c_str(), c == o.s)) o.s = c;
                    ImGui::EndCombo();
                }
            } else {
                input_text(o.name.c_str(), o.s, o.type == "text");
            }
            if(!o.help.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", o.help.c_str());
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::BeginDisabled(!ready() || sel_model.empty() || running);
        if(ImGui::Button("Apply")) apply();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!ready() || !running);
        if(ImGui::Button("Cancel turn")) send({ { "type", "cancel" }, { "chat_id", chat_id } }, "cancel");
        ImGui::EndDisabled();
    }

    void draw_permission() {
        if(!perms.empty() && !perm_open) {
            ImGui::OpenPopup("Permission request");
            perm_open = true;
        }
        if(!perm_open) return;
        if(!ImGui::BeginPopupModal("Permission request", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            perm_open = false; // closed without an answer: reopens next frame while requests remain
            return;
        }
        if(perms.empty()) { // answered elsewhere
            ImGui::CloseCurrentPopup();
            perm_open = false;
        } else {
            const json& p = perms.front();
            ImGui::Text("The agent wants to run: %s", str(p, "name").c_str());
            ImGui::BeginChild("##perm_input", ImVec2(520, 120), ImGuiChildFlags_Borders);
            ImGui::TextWrapped("%s", p.contains("input") ? p["input"].dump(2, ' ', false, json::error_handler_t::replace).c_str() : "");
            ImGui::EndChild();
            if(perms.size() > 1) ImGui::TextDisabled("%zu more waiting", perms.size() - 1);
            if(ImGui::Button("Allow")) answer(true, false);
            ImGui::SameLine();
            if(ImGui::Button("Deny")) answer(false, false);
            ImGui::SameLine();
            if(ImGui::Button("Allow for chat")) answer(true, true);
            if(ImGui::IsKeyPressed(ImGuiKey_Y, false))
                answer(true, false);
            else if(ImGui::IsKeyPressed(ImGuiKey_N, false))
                answer(false, false);
            else if(ImGui::IsKeyPressed(ImGuiKey_A, false))
                answer(true, true);
        }
        ImGui::EndPopup();
    }

    void frame() {
        if(es.state() != smg::EventStream::State::Connected) {
            live = false;
            perms.clear(); // the core denies these when we drop; reopen restores any still pending
        }
        for(const json& m : es.poll()) on_message(m);
        drive_smoke();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImVec2 pos = vp->WorkPos, sz = vp->WorkSize;
        const float lw = sz.x * 0.62f;
        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(lw, sz.y), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Chat")) draw_chat();
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(pos.x + lw, pos.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(sz.x - lw, sz.y * 0.5f), ImGuiCond_FirstUseEver);
        tools.draw("Tool log");

        ImGui::SetNextWindowPos(ImVec2(pos.x + lw, pos.y + sz.y * 0.5f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(sz.x - lw, sz.y * 0.5f), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Controls")) draw_controls();
        ImGui::End();

        draw_permission();
    }
};

std::string default_socket() {
    const char* run = std::getenv("XDG_RUNTIME_DIR");
    return (run && *run ? std::string(run) : "/tmp/pan-deck-" + std::to_string(getuid())) + "/pan-deck.sock";
}

} // namespace

int main(int argc, char** argv) {
    std::string socket = default_socket(), chat_arg;
    Smoke smoke;
    for(int i = 1; i < argc; ++i) {
        const bool more = i + 1 < argc;
        if(!std::strcmp(argv[i], "--socket") && more)
            socket = argv[++i];
        else if(!std::strcmp(argv[i], "--chat") && more)
            chat_arg = argv[++i];
        else if(!std::strcmp(argv[i], "--auto-switch") && more)
            smoke.switch_to = argv[++i];
        else if(!std::strcmp(argv[i], "--auto-send") && more)
            smoke.send = argv[++i];
        else if(!std::strcmp(argv[i], "--auto-allow"))
            smoke.allow = true;
    }

    smg::GuiConfig config;
    config.title = "agentdeck";
    config.size = { 1280, 800 };
    smg::GuiBase gui({ argc, argv }, config);

    DeckClient client(socket, chat_arg);
    client.smoke = smoke;
    gui.add_callback([&client]() {
        client.frame();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    return 0;
}
