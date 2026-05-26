async function api(path, opts = {}) {
  const r = await fetch(path, {
    headers: { Accept: "application/json", "Content-Type": "application/json" },
    ...opts,
  });
  const text = await r.text();
  let data;
  try {
    data = text ? JSON.parse(text) : {};
  } catch {
    data = { raw: text };
  }
  if (!r.ok) {
    throw new Error(data.message || data.detail || text || r.status);
  }
  return data;
}

async function loadHealth() {
  const el = document.getElementById("health");
  const repoEl = document.getElementById("repo-info");
  try {
    const h = await api("/api/health");
    if (h.status === "ok") {
      el.textContent = "Server konfiguriert (Token + Repo).";
      el.className = "status ok";
    } else {
      el.textContent = "Fehler: " + (h.detail || JSON.stringify(h));
      el.className = "status err";
    }
    const ri = await api("/api/repo");
    repoEl.textContent = "Repository: " + ri.full_name;
  } catch (e) {
    el.textContent = "Fehler: " + e.message;
    el.className = "status err";
    repoEl.textContent = "";
  }
}

async function dispatchWorkflow(file) {
  const out = document.getElementById("dispatch-out");
  out.textContent = "Sende…";
  const ref = document.getElementById("ref").value.trim() || "master";
  try {
    const data = await api("/api/dispatch/" + encodeURIComponent(file), {
      method: "POST",
      body: JSON.stringify({ ref }),
    });
    out.textContent = JSON.stringify(data, null, 2);
    loadRuns();
  } catch (e) {
    out.textContent = "Fehler: " + e.message;
  }
}

async function loadRuns() {
  const box = document.getElementById("runs");
  box.innerHTML = "Lade…";
  try {
    const data = await api("/api/runs?per_page=12");
    const runs = data.workflow_runs || [];
    if (!runs.length) {
      box.textContent = "Keine Runs.";
      return;
    }
    box.innerHTML = "";
    runs.forEach((run) => {
      const div = document.createElement("div");
      div.className = "run-item";
      const when = run.updated_at || run.created_at || "";
      const name = run.name || "(Workflow)";
      const conc = run.conclusion || run.status || "";
      div.innerHTML = `<strong>${escapeHtml(name)}</strong> · ${escapeHtml(conc)}<br/><small>${escapeHtml(when)}</small> — <a href="${run.html_url}" target="_blank" rel="noreferrer">öffnen</a>`;
      box.appendChild(div);
    });
  } catch (e) {
    box.textContent = "Fehler: " + e.message;
  }
}

function escapeHtml(s) {
  const d = document.createElement("div");
  d.textContent = s;
  return d.innerHTML;
}

document.querySelectorAll("button[data-workflow]").forEach((btn) => {
  btn.addEventListener("click", () =>
    dispatchWorkflow(btn.getAttribute("data-workflow")),
  );
});

document.getElementById("refresh-runs").addEventListener("click", loadRuns);

loadHealth();
loadRuns();
