from pathlib import Path
p = Path('src/fridge_display.cpp')
s = p.read_text()
old = '''  } else if (showing_errors) {\n    draw_errors(x, y, model);\n  } else if (model.menu_active && model.menu_editing &&\n             model.selected_setting == kAboutSetting) {\n    draw_about(x, y);\n  } else if (model.alarm_active) {\n    draw_alarm(model);\n  } else if (model.menu_active) {'''
new = '''  } else if (showing_errors) {\n    draw_errors(x, y, model);\n  } else if (model.alarm_active) {\n    draw_alarm(model);\n  } else if (model.menu_active && model.menu_editing &&\n             model.selected_setting == kAboutSetting) {\n    draw_about(x, y);\n  } else if (model.menu_active) {'''
if s.count(old) != 1:
    raise SystemExit(f'expected one display-priority block, found {s.count(old)}')
p.write_text(s.replace(old, new, 1))
