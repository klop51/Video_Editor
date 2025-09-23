#pragma once
#include <QPointer>
#include <QMetaObject>
#include <QWidget>
#include <atomic>

namespace ui {
// Drop-in repaint guard: prevents synchronous repaint storms and cross-thread misuse.
struct UpdateGate {
  std::atomic_bool pending{false};
  QPointer<QWidget> w;
  explicit UpdateGate(QWidget* widget=nullptr) : w(widget) {}
  void bind(QWidget* widget) { w = widget; }
  void queueUpdate() {
    if (!w) return;
    bool expected = false;
    if (!pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    // Always run on widget's thread.
    QMetaObject::invokeMethod(w, [this]{
      if (!w) { pending.store(false, std::memory_order_release); return; }
      w->update();                       // schedule paint
      pending.store(false, std::memory_order_release);
    }, Qt::QueuedConnection);
  }
};
} // namespace ui