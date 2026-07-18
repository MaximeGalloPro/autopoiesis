from pathlib import Path


root = Path(__file__).resolve().parents[1]
god_prompt = (root / "scripts" / "god-agent.sh").read_text(encoding="utf-8")
correction_prompt = (root / "scripts" / "god-correction-agent.sh").read_text(encoding="utf-8")
verifier = (root / "scripts" / "verify-evolution.sh").read_text(encoding="utf-8")

for prompt in (god_prompt, correction_prompt):
    assert "ne lance pas Docker" in prompt
    assert "verificateur externe" in prompt

assert 'docker compose -f "$WORKTREE/compose.yaml" build' in verifier
