#pragma once

#include <any>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <stdexcept>
#include <vector>

struct Vertex {
  const uint64_t id;
  const std::any data;

  template <typename T>
  Vertex(uint64_t id, T&& data) :
      id(id),
      data(std::forward<T>(data)) {}

  template <typename T>
  T& Data() {
    return std::any_cast<T>(data);
  }
};

struct Edge {
  const uint64_t id;
  const uint64_t from;
  const uint64_t to;

  Edge(uint64_t id, uint64_t from, uint64_t to, bool directed) :
      id(id),
      from{from},
      to{to} {}
};

struct Graph final {
  std::vector<Vertex> vertices;
  std::vector<Edge> edges;
  std::map<uint64_t, std::vector<uint64_t>> adjacency_list;

  explicit Graph(std::size_t V) {
    vertices.reserve(V);
  }
  ~Graph() noexcept = default;

  template <typename T>
  uint64_t AddVertex(T&& data) {
    auto id = vertices.size();
    vertices.emplace_back(id, std::forward<T>(data));
    return id;
  }

  Vertex& Vertex(uint64_t vertex_id) {
    return vertices.at(vertex_id);
  }
  Edge& Edge(uint64_t edge_id) {
    return edges.at(edge_id);
  }

  uint64_t AddEdge(uint64_t from, uint64_t to, bool directed) {
    auto id = edges.size();
    edges.emplace_back(id, from, to, directed);
    adjacency_list[from].emplace_back(to);
    return id;
  }

  const auto& Neighbors(uint64_t vertex_id) const {
    static const std::vector<uint64_t> empty;
    auto it = adjacency_list.find(vertex_id);
    if (it != adjacency_list.end()) {
      return it->second;
    }
    return empty;
  }
};

struct TopoOrder {
  std::vector<std::vector<uint64_t>> layers;
  std::vector<std::size_t> layer_of;
};

inline TopoOrder TopoSort(const Graph& graph) {
  std::vector<uint64_t> in_degree(graph.vertices.size(), 0);
  for (const auto& edge : graph.edges) {
    in_degree[edge.to]++;
  }

  std::queue<uint64_t> zero_in_degree;
  for (uint64_t i = 0; i < in_degree.size(); ++i) {
    if (in_degree[i] == 0) {
      zero_in_degree.push(i);
    }
  }
  if (zero_in_degree.empty())
    throw std::runtime_error("Cycle detected");

  std::vector<std::vector<uint64_t>> layers;
  std::vector<std::size_t> layer_of(graph.vertices.size(), 0);
  layers.push_back({});
  std::size_t curr = zero_in_degree.size();
  std::size_t next = 0;
  while (!zero_in_degree.empty()) {
    uint64_t vertex = zero_in_degree.front();
    zero_in_degree.pop();
    curr--;
    layers.back().push_back(vertex);
    layer_of[vertex] = layers.size() - 1;

    for (uint64_t neighbor : graph.Neighbors(vertex)) {
      in_degree[neighbor]--;
      if (in_degree[neighbor] == 0) {
        zero_in_degree.push(neighbor);
        next++;
      }
    }
    if (curr == 0) {
      curr = next;
      next = 0;
      layers.push_back({});
    }
  }

  return {std::move(layers), std::move(layer_of)};
}
