// Graph.h
#pragma once

#include <cstdint>
#include <stdexcept>
#include <limits>

/// Interfaccia polimorfica per grafi (semplici o ipergrafi).
class Graph {
public:
    using vertex_t = uint32_t;
    using edge_t   = uint32_t;
    static constexpr vertex_t INVALID_VERTEX = std::numeric_limits<vertex_t>::max();
	static constexpr vertex_t INVALID_EDGE = std::numeric_limits<edge_t>::max();

    virtual ~Graph() = default;

    /// Numero di vertici (o nodi) nel grafo.
    virtual vertex_t number_of_vertices() const = 0;

    /// Numero di "archi": 
    ///   - per UndirectedGraph = numero di edges
    ///   - per Hypergraph       = numero di hyperedges
    virtual edge_t number_of_edges() const = 0;

    /// Grado del vertice v:
    ///   - nei grafi semplici = numero di vicini
    ///   - negli ipergrafi    = numero di iperarcs incidenti
    virtual vertex_t degree(vertex_t v) const = 0;

    /// k-esimo "arco" incidente a v:
    ///   - negli UndirectedGraph restituisce il k-esimo vicino.
    ///   - negli Hypergraph       restituisce l’id dell’iperarco k-esimo incidente.
    virtual edge_t incident_edge(vertex_t v, vertex_t k) const = 0;

    virtual void prefault();
};