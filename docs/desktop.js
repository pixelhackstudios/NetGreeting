(() => {
  const desktop = document.getElementById("desktop");
  const tasksEl = document.getElementById("tasks");
  const startBtn = document.getElementById("startBtn");
  const startMenu = document.getElementById("startMenu");
  const windows = [...document.querySelectorAll(".win")];

  let z = 10;
  let activeId = null;

  const state = new Map();

  function deskSize() {
    return { w: desktop.clientWidth, h: desktop.clientHeight };
  }

  function saveRect(win) {
    state.set(win.id, {
      left: win.style.left,
      top: win.style.top,
      width: win.style.width,
      height: win.style.height || win.offsetHeight + "px",
    });
  }

  function focusWin(win) {
    if (!win || win.classList.contains("closed")) return;
    win.classList.remove("minimized");
    windows.forEach((w) => w.classList.toggle("active", w === win));
    win.style.zIndex = String(++z);
    activeId = win.id;
    syncTasks();
  }

  function isOpen(win) {
    return !win.classList.contains("closed") && !win.classList.contains("minimized");
  }

  function syncTasks() {
    tasksEl.innerHTML = "";
    windows.forEach((win) => {
      if (win.classList.contains("closed")) return;
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "task" + (win.id === activeId && isOpen(win) ? " active" : "");
      btn.textContent = win.dataset.title || "Window";
      btn.addEventListener("click", () => {
        if (win.classList.contains("minimized")) {
          win.classList.remove("minimized");
          focusWin(win);
        } else if (win.id === activeId) {
          minimize(win);
        } else {
          focusWin(win);
        }
      });
      tasksEl.appendChild(btn);
    });
  }

  function minimize(win) {
    win.classList.add("minimized");
    win.classList.remove("active");
    if (activeId === win.id) {
      activeId = null;
      const next = [...windows].reverse().find(isOpen);
      if (next) focusWin(next);
      else syncTasks();
    } else {
      syncTasks();
    }
  }

  function closeWin(win) {
    win.classList.add("closed", "minimized");
    win.classList.remove("active", "maximized");
    if (activeId === win.id) {
      activeId = null;
      const next = [...windows].reverse().find(isOpen);
      if (next) focusWin(next);
      else syncTasks();
    } else {
      syncTasks();
    }
  }

  function restoreWin(win) {
    win.classList.remove("closed", "minimized");
    focusWin(win);
  }

  function toggleMax(win) {
    if (win.classList.contains("maximized")) {
      const s = state.get(win.id);
      win.classList.remove("maximized");
      if (s) {
        win.style.left = s.left;
        win.style.top = s.top;
        win.style.width = s.width;
        win.style.height = s.height;
      }
    } else {
      saveRect(win);
      win.style.height = win.offsetHeight + "px";
      win.classList.add("maximized");
    }
    focusWin(win);
  }

  function clamp(win) {
    const { w, h } = deskSize();
    const rect = { l: win.offsetLeft, t: win.offsetTop, ww: win.offsetWidth, hh: win.offsetHeight };
    const title = 24;
    if (rect.l + rect.ww < 80) win.style.left = 80 - rect.ww + "px";
    if (rect.t < 0) win.style.top = "0px";
    if (rect.l > w - 80) win.style.left = w - 80 + "px";
    if (rect.t > h - title) win.style.top = h - title + "px";
  }

  function startDrag(win, e) {
    if (e.button !== 0 && e.pointerType === "mouse") return;
    if (win.classList.contains("maximized")) {
      const s = state.get(win.id);
      const width = s ? parseInt(s.width, 10) : 280;
      win.classList.remove("maximized");
      if (s) {
        win.style.width = s.width;
        win.style.height = s.height;
      }
      win.style.left = Math.max(0, e.clientX - width / 2) + "px";
      win.style.top = "0px";
    }
    focusWin(win);
    const ox = e.clientX - win.offsetLeft;
    const oy = e.clientY - win.offsetTop;
    win.setPointerCapture(e.pointerId);
    const move = (ev) => {
      win.style.left = ev.clientX - ox + "px";
      win.style.top = ev.clientY - oy + "px";
    };
    const up = () => {
      win.removeEventListener("pointermove", move);
      win.removeEventListener("pointerup", up);
      clamp(win);
      saveRect(win);
    };
    win.addEventListener("pointermove", move);
    win.addEventListener("pointerup", up);
  }

  function startResize(win, e) {
    e.stopPropagation();
    if (win.classList.contains("maximized")) return;
    focusWin(win);
    const startX = e.clientX;
    const startY = e.clientY;
    const startW = win.offsetWidth;
    const startH = win.offsetHeight;
    win.setPointerCapture(e.pointerId);
    const move = (ev) => {
      const w = Math.max(180, startW + (ev.clientX - startX));
      const h = Math.max(80, startH + (ev.clientY - startY));
      win.style.width = w + "px";
      win.style.height = h + "px";
    };
    const up = () => {
      win.removeEventListener("pointermove", move);
      win.removeEventListener("pointerup", up);
      saveRect(win);
    };
    win.addEventListener("pointermove", move);
    win.addEventListener("pointerup", up);
  }

  windows.forEach((win) => {
    saveRect(win);
    win.addEventListener("mousedown", () => focusWin(win));
    const bar = win.querySelector("[data-drag]");
    bar.addEventListener("pointerdown", (e) => {
      if (e.target.closest("button")) return;
      startDrag(win, e);
    });
    bar.addEventListener("dblclick", (e) => {
      if (e.target.closest("button")) return;
      toggleMax(win);
    });
    win.querySelector("[data-min]").addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();
      minimize(win);
    });
    win.querySelector("[data-max]").addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();
      toggleMax(win);
    });
    win.querySelector("[data-close]").addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();
      closeWin(win);
    });
    const grip = win.querySelector("[data-resize]");
    if (grip) {
      grip.addEventListener("pointerdown", (e) => startResize(win, e));
    }
  });

  document.querySelectorAll("[data-open]").forEach((el) => {
    el.addEventListener("click", (e) => {
      const id = el.getAttribute("data-open");
      const win = document.getElementById(id);
      if (!win) return;
      if (el.tagName === "A") return;
      e.preventDefault();
      restoreWin(win);
    });
  });

  document.querySelectorAll(".desk-icon").forEach((icon) => {
    icon.addEventListener("click", () => {
      document.querySelectorAll(".desk-icon").forEach((i) => i.classList.remove("sel"));
      icon.classList.add("sel");
    });
  });

  startBtn.addEventListener("click", (e) => {
    e.stopPropagation();
    const open = startMenu.hidden;
    startMenu.hidden = !open;
    startBtn.setAttribute("aria-expanded", String(open));
  });

  document.addEventListener("click", (e) => {
    if (e.target.closest(".start-menu") || e.target.closest(".start")) return;
    startMenu.hidden = true;
    startBtn.setAttribute("aria-expanded", "false");
  });

  function tick() {
    document.getElementById("clock").textContent = new Date().toLocaleTimeString([], {
      hour: "numeric",
      minute: "2-digit",
    });
  }
  tick();
  setInterval(tick, 10000);

  // Only pull a window back if it has gone fully off the desktop.
  function fitCluster() {
    const { w, h } = deskSize();
    windows.forEach((win) => {
      if (win.classList.contains("maximized")) return;
      if (win.offsetLeft > w - 48) win.style.left = Math.max(96, w - win.offsetWidth - 8) + "px";
      if (win.offsetTop > h - 24) win.style.top = Math.max(8, h - 24) + "px";
      saveRect(win);
    });
  }

  focusWin(document.getElementById("win-call"));
  fitCluster();
  window.addEventListener("resize", fitCluster);
})();
