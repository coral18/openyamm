# Local HTML preview capture

Use the reusable executable from the repository root:

```sh
./tools/capture_local_html.py level_generation/creatures/mm6_pmn2/review.html \
  level_generation/creatures/mm6_pmn2/review/viewer_capture.png \
  --query 'action=walk&view=6&palette=803&frame=3'
```

It runs headless Chrome with a fresh temporary profile, waits for the page's virtual-time budget, checks that a new
PNG was produced, and copies it to the requested destination. HTML and PNG paths must be inside this repository or
`/tmp`. It accepts `--width`, `--height`, `--wait-ms` and `--query`; no arbitrary Chrome flags are forwarded. It does
not attach to the user's browser. Chrome/Chromium must already be installed.

The driver approved the command prefix **`["./tools/capture_local_html.py"]`** on 2026-09-08. Agents should call that
executable directly from the repository root, changing its arguments as needed. Do not replace it with inline Chrome
commands, a `bash -lc` wrapper, environment assignments, heredocs or compound shell commands: those can require new
approvals for each capture. If the environment requires escalation, use this same narrow prefix through the execution
tool. Approval is enforced by the tool, not this markdown; another environment may need one initial approval.

`--wait-ms` is Chrome virtual time, not a reliable claim about elapsed `requestAnimationFrame` animation time.
For a specific animation pose, use the viewer's explicit frame query when supported. The pmn2 viewer accepts
zero-based `frame=N` and pauses on that native sequence entry. Inspect captures before accepting them; a valid PNG
alone does not prove the page's artwork or animation is correct.
