/*
Created: 18:08:2026 - 17:36:58
Last updated: 28:08:2026 - 19:30:00
Module: engine/app
File: engine/app/sources/AppAfterFrame.cpp

Responsibility:
- EVERYTHING THAT MUST HAPPEN AFTER render(), AND ONLY BECAUSE OF THAT. The
  trajectory row, the telemetry sample, the state capture, the chat entry, the
  close-after-flush countdown, the restore report, the body probe and the
  tour's settle gate. They have exactly one thing in common, and this file
  exists to say it out loud rather than leave it as an accident of line order.

Key items:
- App::after_frame(): the whole tail of one frame, in the order it must run.

Dependencies:
- Uses: App.h and AppAfterFrame.h (the gates). Nothing new.
- Used by: App.cpp (one call at the end of the loop body).

Notes:
- WHY THEY ARE TOGETHER, and it is the only reason. Each of these needs the
  frame to have been PRESENTED:
    * the trajectory row records the eye pose the frame was drawn from, so a
      replay reproduces the image rather than something near it;
    * the capture writes a .png and a sidecar that must describe the SAME
      frame -- collected before render() it would save the state of frame N
      beside the image of frame N-1, which is the kind of small lie that makes
      a repro fail for reasons nobody can find;
    * the chat entry carries that capture, so it inherits the same rule;
    * the close-after-flush countdown exists because save_screenshot() returns
      when the capture is REQUESTED, not when the file exists;
    * the tour's gate reads queues that this frame's render has just drained.
  Nothing here is grouped by subject matter, and grouping it by subject matter
  would be the mistake: split the capture from its countdown and the pair goes
  wrong in a way that produces a .txt with no .png beside it, which is what
  happened once already.
- THE DECISIONS ARE NEXT DOOR, IN THE HEADER. What is left here is side
  effects: files, the renderer, the window. Those cannot be measured without a
  window; the four things this code CHOOSES can, and they are inline in
  AppAfterFrame.h with tests/app/AfterFrameTests.cpp on them.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 18:08:2026 - 17:36:58: Создан. Слой 4 разбора App.cpp: хвост кадра (165 строк) уехал
  из run() сюда, вместе с доводом, по которому он там был.
- 20:08:2026 - 15:30:00: Лента прохода DFN_RECORD_EVERY: каждый N-й показанный кадр — .png + строка rec.log тем же снимком, что F2.
- 28:08:2026 - 19:30:00: Затвор один на всех, и у него появились две недостающие очереди:
  ЧАНКИ (ChunkManager::pending_chunk_count — та самая «честная дыра», записанная
  здесь же) и СХОДИМОСТЬ КАПСУЛЫ (PLAYER_SETTLE_EPS_M). Признак «мир успокоился»
  выводился из событий ChunkLoaded за кадр — то есть отвечал «что-то приехало» на
  вопрос «очередь пуста».
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/AppAfterFrame.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace dfn::app {

void App::after_frame(float alpha, float frame_dt) {


    // TRAJECTORY RECORDING (O3): every PRESENTED frame's eye pose + counted
    // clock + fov, so replay reproduces the image exactly. Recording is an
    // editor action, but a DFN_TRAJ_REC door (armed at enter_world) records
    // any mode, so this is gated on the recorder, not the mode.
    if (traj_rec_.active()) {
        const auto eye = camera_.interpolated_pose(alpha);
        TrajectoryFrame tf;
        tf.game_seconds = game_seconds_;
        tf.position = eye.position;
        tf.yaw = eye.yaw;
        tf.pitch = eye.pitch;
        tf.fov_y = camera_.fov_y();
        traj_rec_.push(tf);
    }

    // TELEMETRY RING (item 3), EDITOR ONLY -- in-game stays light (В39: no
    // continuous log). One sample every 1/TELEMETRY_LOG_HZ of COUNTED time,
    // so the log of a given walk has the same length on any machine. Reuses
    // collect_snapshot. Flushed beside the map on stop.
    if (mode_ == AppMode::Editor) {
        if (telemetry_due(game_seconds_, telemetry_last_s_,
                          static_cast<double>(config::TELEMETRY_LOG_HZ))) {
            telemetry_last_s_ = game_seconds_;
            const DebugSnapshot s = collect_snapshot(alpha);
            TelemetrySample t;
            t.game_seconds = s.game_seconds;
            t.position = s.position;
            t.yaw = s.yaw;
            t.pitch = s.pitch;
            t.fps = s.fps;
            t.frame_ms = s.frame_ms;
            t.chunks_resident = s.chunks_resident;
            t.lod_nodes = s.lod_nodes;
            // triangles / aim_target: render seam, left 0/"" until a hook
            // fills them (read-if-present, never a block here).
            telemetry_.push(t);
        }
    }

    // ЛЕНТА ПРОХОДА (DFN_RECORD_EVERY): каждый N-й показанный кадр — .png и
    // строка состояния. После render() по той же причине, что и захват ниже:
    // картинка и её числа обязаны быть ОДНИМ кадром. Числа — те же, что у F2
    // (collect_snapshot): позиция глаза, взгляд, скорость, аллюр, опора.
    if (record_every_ > 0) {
        ++record_seen_;
        if (record_seen_ % record_every_ == 0) {
            const DebugSnapshot s = collect_snapshot(alpha);
            char name[64];
            std::snprintf(name, sizeof(name), "rec_%05d.png", record_written_);
            const std::string dir =
                capture_dir_.empty() ? std::string("screenshots") : capture_dir_;
            if (renderer_->save_screenshot(dir + "/" + name)) {
                if (std::FILE* f = std::fopen((dir + "/rec.log").c_str(), "ab");
                    f != nullptr) {
                    std::fprintf(
                        f,
                        "%d t=%.3f pos=(%.2f %.2f %.2f) yaw=%.1f pitch=%.1f "
                        "look=(%.2f %.2f %.2f) v=%.2f gait=%u ground=%d\n",
                        record_written_, s.game_seconds,
                        static_cast<double>(s.position.x),
                        static_cast<double>(s.position.y),
                        static_cast<double>(s.position.z),
                        static_cast<double>(s.yaw * 57.29578f),
                        static_cast<double>(s.pitch * 57.29578f),
                        static_cast<double>(s.look_dir.x),
                        static_cast<double>(s.look_dir.y),
                        static_cast<double>(s.look_dir.z),
                        static_cast<double>(s.speed_mps),
                        static_cast<unsigned>(s.gait), s.grounded ? 1 : 0);
                    std::fclose(f);
                }
                ++record_written_;
            }
        }
    }

    // CAPTURE AFTER RENDER, so the .png and the sidecar are the same frame.
    // The snapshot is collected a second time here rather than reused from
    // the overlay above -- one frame of drift between the image and its
    // state file is exactly the kind of small lie that makes a repro fail
    // for reasons nobody can find.
    if (capture_pending_) {
        capture_pending_ = false;
        write_capture(collect_snapshot(alpha));
        // CLOSING HERE WOULD LOSE THE PNG. save_screenshot() returns true
        // when the capture has been REQUESTED, not when the file exists --
        // the bgfx backend reads the framebuffer back over the following
        // frames. Closing on the same frame produced a .txt with no .png
        // beside it, and, worse, a "[capture] ok" line above the pair. So
        // the tooling door waits for the flush; the same reason the body
        // probe holds a 4-frame cooldown between shots.
        if (capture_then_close_) {
            flush_countdown_.arm();
        }
    }
    // CHAT ENTRY AFTER RENDER, same reason as the capture: the attached
    // capture and the entry must describe the same frame. The DFN_CHAT_MSG
    // door waits for the backend flush before closing (the png lands over
    // the following frames), like F2.
    if (chat_pending_) {
        chat_pending_ = false;
        write_pending_chat(alpha);
        if (chat_then_close_) {
            flush_countdown_.arm();
        }
    }
    // How far the restore actually got. IPhysics has no teleport (see
    // apply_restore), so this is the check that keeps a half-completed
    // restore from passing as a completed one.
    if (flush_countdown_.tick()) {
        window_->request_close();
    }
    if (restore_target_) {
        if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
            const glm::vec3 got = physics_->character_position(ps->character);
            // ГОРИЗОНТАЛЬ И ВЕРТИКАЛЬ — РАЗНЫЕ ВЕЛИЧИНЫ, и отказом является
            // только одна: осадка капсулы на землю это работа контроллера, а
            // не промах. Само это решение живёт в AppAfterFrame.h и там же
            // проверяется — оно уже было однажды принято на неверной величине
            // и кричало бы «заблокировано» на каждом восстановлении.
            const RestoreLanding land = restore_landing(got, *restore_target_);
            std::fprintf(stderr,
                         "[restore] landed %.2f %.2f %.2f  horiz %.3f m  "
                         "settle %.3f m%s\n",
                         static_cast<double>(got.x), static_cast<double>(got.y),
                         static_cast<double>(got.z),
                         static_cast<double>(land.horiz_m),
                         static_cast<double>(land.settle_m),
                         land.blocked
                             ? "  -- BLOCKED, this is NOT the captured spot"
                             : "");
        }
        restore_target_.reset();
    }
    body_probe_frame(alpha, frame_dt);
    // THE TOUR'S SETTLE IS GATED ON THE WORLD HAVING STOPPED CHANGING,
    // not on frames elapsing. `Tour.cpp` waits a fixed 45 RENDERED FRAMES
    // for streaming that is driven in SIM STEPS off a wall clock -- Rule 42,
    // a budget denominated in one clock's units enforcing a limit that only
    // matters in another's. The cost was measured, not guessed: two runs of
    // the SAME binary at the SAME commit differ by 17.4% of pixels (34.7%
    // re-measured later), so no full-tour pixel claim below ~20% has ever
    // certified anything -- in the instrument this project uses for Rule 27.
    //
    // The gate lives HERE rather than in Tour.cpp because the app is the
    // only place that can see all three queues at once, and because it needs
    // no change to render's contract: withholding the call simply pauses the
    // countdown, so the 45 frames now run on a SETTLED world instead of
    // starting at the refocus.
    //
    // HYSTERESIS IS NOT OPTIONAL: a queue legitimately reads empty for one
    // frame mid-refocus, so quiescence must HOLD. And the cap is a backstop
    // that REPORTS -- an unreachable vantage must say so rather than hang,
    // because a tour that quietly never finishes is the same silent-zero
    // failure as a capture that wrote nothing.
    //
    // HONEST GAP, disclosed rather than papered over: there is no
    // chunk-pending accessor, so "chunks still arriving" is inferred from
    // ChunkLoaded/ChunkUnloaded events seen this frame. That is sound for
    // "something arrived" and blind to "something is queued and has not
    // arrived yet" -- a request to core is out for the real counter, and
    // until it lands the cap is doing more work than it should.
    //
    // ЧЕТВЁРТАЯ ОЧЕРЕДЬ — ОЧЕРЕДЬ ЧАНКОВ, и она была той самой «честной
    // дырой», записанной выше: `pending_chunk_count` появился в
    // engine/world 29.08 ровно по этой заявке. До него кадр, на котором
    // ничего не приехало, читался успокоившимся, даже когда в радиусе не
    // хватало пятнадцати клеток, — потому что признак был «событий не
    // было», а не «очередь пуста».
    //
    // ПЯТОЕ УСЛОВИЕ — КАПСУЛА ИГРОКА. Восстановление ставит игрока в точку,
    // а контроллер потом ОСАЖИВАЕТ его на пол; замер дверной волны — 1.3 мм
    // между прогонами. Осадка сходится за считанные шаги, поэтому здесь она
    // не «выключена», а ДОЖДАНА: порог в одну десятую миллиметра за кадр.
    // Порог, а не равенство: контроллер и на сошедшейся позе шевелит
    // последний бит.
    //
    // ЗАТВОР ОДИН НА ВСЕХ. Раньше этот блок стоял целиком внутри
    // `if (tour_.active())`, и дверь дозы DFN_CAPTURE_AFTER_FRAMES — та,
    // которой снимают города, — не ждала мир вообще: она отсчитывала свои
    // 120 кадров от запуска и снимала то, что успело приехать. Вердикт
    // считается КАЖДЫЙ кадр и латчится, потому что его спрашивают трое:
    // тур, дверь дозы и часы мира (они стоят, пока затвор ждёт).
    const bool shutter_armed = tour_.active() || capture_after_frames_ > 0;
    if (!shutter_armed) {
        // НИКТО НЕ ЖДЁТ — И СПРАШИВАТЬ НЕЧЕГО. Гонять затвор в живой игре
        // значило бы каждый кадр пересчитывать очередь ради ответа, который
        // никто не читает, и вдобавок печатать «мир не успокоился» на шестисотом
        // кадре обычной ходьбы: мир в игре не обязан успокаиваться вовсе.
        world_settled_ = true;
        settle_gate_ = SettleGate{};
        settle_cap_said_ = false;
    } else {
        float settle_step = 0.0f;
        if (const auto* ps = world_.get<gameplay::PlayerState>(player_);
            ps != nullptr && physics_ != nullptr) {
            const float y = physics_->character_position(ps->character).y;
            settle_step = std::fabs(y - last_player_y_);
            last_player_y_ = y;
        }
        const bool quiet = !world_changed_this_frame_
                           && chunks_pending_ == 0
                           && chunks_.coarse_pending_count() == 0
                           && render_system_.lod_pending().empty()
                           && settle_step <= PLAYER_SETTLE_EPS_M;
        // ГИСТЕРЕЗИС И ПОТОЛОК — В AppAfterFrame.h, где их можно прогнать без
        // окна. Здесь остаётся ровно то, что окна требует: спросить очереди и
        // напечатать, если сдались.
        const SettleGate::Verdict v = settle_gate_.observe(quiet);
        world_settled_ = v.shoot;
        if (v.capped && !v.settled && !settle_cap_said_) {
            settle_cap_said_ = true;
            std::fprintf(stderr,
                         "[затвор] мир не успокоился за %d кадров "
                         "(тихих %d, чанков %zu, крупных %zu, дальних %zu, "
                         "осадка %.4f мм) -- снимаю всё равно, ЭТОТ КАДР НЕ "
                         "ДОКАЗАТЕЛЬСТВО\n",
                         settle_gate_.unsettled_frames, settle_gate_.quiet_frames,
                         chunks_pending_, chunks_.coarse_pending_count(),
                         render_system_.lod_pending().size(),
                         static_cast<double>(settle_step) * 1000.0);
        }
        if (tour_.active() && v.shoot && tour_.post_frame(*renderer_)) {
            window_->request_close(); // tour finished (render's contract)
        }
    }
    if (playtest_ && playtest_->finished && pt_artifacts_pending_) {
        gameplay::playtest_write_artifacts(*playtest_, pt_dir_);
        pt_artifacts_pending_ = false;
        window_->request_close();
    }

}

} // namespace dfn::app
