// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

// Read-only USD queries registered into the QueryRegistry. Each answers with
// JSON, reports a missing document or prim through CommandError, and caps the
// number of rows it returns so a chatty agent cannot blow up its own context
// window.
class QueryRegistry;

void registerStageQueries(QueryRegistry& registry);
