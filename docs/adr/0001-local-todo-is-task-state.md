# ADR-0001: TODO is the task-state source of truth

## Status

Accepted

## Decision

TODO.md is the sole current-status source. Specifications, ticket details, PRs, HIL reports, and learning material link to tasks without duplicating their status. Cross-repository work uses linked IDs in both repositories.

## Consequences

Local workflow tools and CI validate the TODO contract. Changes remain reviewable in Git and no issue-tracker account is needed to work a task.
