# 🛠️ CS 537 – TA Development Repository (`p-dev`)

Welcome to the TA development space for CS 537 project infrastructure.  
This repository (`p1-dev`) is private where TAs can freely design, develop, and iterate on assignments before publishing them to students.

---

## 📁 Repository Structure

Each assignment is organized under its own subgroup, like `fall26/p1/`, with the following layout:

```

fall26/
└── p1/
├── p1-base/ # Public-facing base repository used by students (forked by each)
├── p1-dev/ # Private TA-only development repository (this repo)
└── students/
    ├── p1-netid1/ # Private per-student repositories (forks of p1-base)
    ├── p1-netid2/
    └── ...

```

---

## 🚧 Workflow for Assignment Development

### Step 1: Work in `p1-dev`

- Use this repo as your scratchpad for designing writeups, tests, and starter code.
- Keep work organized in folders, just like it will appear in `p1-base`.

### Step 2: Publish to `p1-base`

- Once finalized, **copy the contents** from `p1-dev` into `p1-base`.
- Push directly to the `main` branch in `p1-base` — this will serve as the **public version**.
- Run the `setup_student_repos.py` script to create the student private repositories which will fork from the `p1-base` repository.  For assignments that are team assignments, run the script `setup_team_repos.py`, which will create a repo for each team and grant access to all team members.  Since this is a fork of the base repository, all files and history from the base repository are visible to the students in their own private repositories.

> 🧠 Students fork's are created from `p1-base` and receive updates either through GitLab’s UI (explained in `base_readme.md`).

- If, after forking the student repos, you update the p1-base repository, students will need to pull to merge the changes that were made to the base repository into their personal repositories.
---

## 🤝 Student Contributions

Student-submitted contributions to the assignment are handled through the `contributions` branch in `p1-base` repository.

### Review & Merge Process:

1. TAs are notified via merge requests to the `contributions` branch in individual student repositories.
2. After reviewing these requests, TAs can merge them into the `contributions` branch in `p1-base`.
3. After that TAs can work in the contributions branch of `p1-base`, copy relevant files out of the repo and add contributions to the project in the `main` branch.

> ⚠️ Always inspect student-submitted content for academic integrity and quality before accepting.

---

## 🧪 Testing & Grading

_(Coming soon...)_

This section will outline:

- A script to run to pull from all student repos, the last commit before the due date (also handling slip days).
- How to structure and run the official test suite.
- How to assign weights to tests.
- How to generate grading output (e.g., CSVs for Canvas).

---

## 🔐 Permissions

| Role            | Access                                       |
| --------------- | -------------------------------------------- |
| **TAs**         | Maintainer access to all repos               |
| **Instructors** | Owner access for full administrative control |
| **Students**    | Developer on their own repo only (via fork)  |

---

## 💡 Tips & Best Practices

- Keep assignment structure consistent across projects.
- Ensure test cases are deterministic and minimal.
- Version important changes via tags or protected branches (e.g., `release-v1.0`).
- Periodically verify that student forks are up-to-date via the bot logs.

---

## 🧹 Cleanup & Semester Rollover

At the end of the semester:

- Archive `p1-dev` and `p1-base` with tags (e.g., `fall26-final`)
- Disable pushes to `main` in `p1-base`
- Clear stale student forks if needed

---

For any questions or enhancements, reach out to the course staff group chat or open a private issue in this repo.
