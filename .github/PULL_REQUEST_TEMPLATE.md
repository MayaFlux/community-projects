## Project

Project path: `<author>/<theme>/<project-name>/`

Briefly describe what your project does and how to run it:


## Project README (recommended)

- [ ] I added a `README.md` inside the project directory with a short description and run instructions.
- [ ] I included screenshots or GIFs, and links to any demo videos, to show the project in action.

Images and video links are optional, but they help visitors browse the projects as a gallery.

## Optional project metadata

- [ ] I added `project.json` beside the project files.
- [ ] I am leaving metadata out; the minimum compatible MayaFlux version will be shown as unknown.

If you add metadata, put it at `<author>/<theme>/<project-name>/project.json`:

```json
{
  "name": "Project name",
  "themes": ["theme"],
  "description": "A short description of the project.",
  "min_mayaflux_version": "0.0.0"
}
```

The project files can use any layout; `src/` and `shaders/` are suggestions.
