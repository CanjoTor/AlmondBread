#include <SFML/Graphics.hpp>
#include <complex>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
#include <chrono>
#include <mutex>
#include <fstream>
#include <iostream>
#include <vector>

using Complex = std::complex<double>;
std::mutex output_mutex;

int mandelbrot(Complex const &c)
{
  int i = 0;
  auto z = c;
  for (; i != 256 && norm(z) < 4.; ++i)
  {
    z = z * z + c;
  }
  return i;
}

auto to_color(int k)
{
  return k < 256 ? sf::Color{static_cast<sf::Uint8>(10 * k), 0, 0}
                 : sf::Color::Black;
}

void color_continious_picture(double *array, int width, int height, std::string &str)
{
  // Calculate the min and max value of the normalized array

  double min = array[0];
  double max = array[0];
  for (int i = 0; i < width * height; i++)
  {
    if (array[i] < min)
    {
      min = array[i];
    }
    if (array[i] > max)
    {
      max = array[i];
    }
  }

  sf::Image image;
  image.create(width, height);

  // Create the image of the runtime per cell

  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      double tmp = (array[i * width + j] - min) / (max - min) * 255 * 10; // Normalize the value between 0 and 255 and multyply by 10 for visability
      tmp = std::max(0.0, std::min(2550.0, tmp)); // Clamp the value between 0 and 2550

      auto clamped_tmp = std::min(tmp, 255.0);
      auto color_temp = sf::Color{static_cast<sf::Uint8>(clamped_tmp), 0, 0};

      image.setPixel(j, i, color_temp);
    }
  }
  image.saveToFile(str);
}

int main()
{
  int const display_width{1000};
  int const display_height{1000};

  Complex const top_left{-2.2, 1.5};
  Complex const lower_right{0.8, -1.5};
  auto const diff = lower_right - top_left;

  auto const delta_x = diff.real() / display_width;
  auto const delta_y = diff.imag() / display_height;

  sf::Image mandelbrot_image;

  double array[display_width * display_height] = {0}; // Array to store the time per pixel

  double t = 0;

  mandelbrot_image.create(display_width, display_height);

  std::ofstream file("../output/output.txt", std::ios::app);

  for (std::size_t grain_size : {1, 2, 3, 4, 5, 10, 20, 40, 70,  100, 200, 400, 600, 1000})
  {

    tbb::parallel_for(
        tbb::blocked_range2d<int>(0, display_height, grain_size, 0, display_width, grain_size),
        [&](const tbb::blocked_range2d<int> &range)
        {
          auto start = std::chrono::high_resolution_clock::now();
          for (int row = range.rows().begin(); row < range.rows().end(); ++row)
          {
            for (int column = range.cols().begin(); column < range.cols().end(); ++column)
            {
              auto start_tmp = std::chrono::high_resolution_clock::now(); // Start the timer for each pixel

              auto k = mandelbrot(top_left + Complex{delta_x * column, delta_y * row});
              mandelbrot_image.setPixel(column, row, to_color(k));

              // Time measurement for each pixel
              auto stop_tmp = std::chrono::high_resolution_clock::now();
              auto duration_tmp = std::chrono::duration_cast<std::chrono::nanoseconds>(stop_tmp - start_tmp);
              double t_tmp = duration_tmp.count();

              array[row * display_width + column] = t_tmp; 
            }
          }
          // Time measurement for each grain_size
          auto stop = std::chrono::high_resolution_clock::now();
          auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);

          t = duration.count();

          // {
          //   std::lock_guard<std::mutex> lock(output_mutex);
          //   static int block_id = 0;
          //   block_timings.emplace_back(block_id++, duration.count());
          //   // std::cout << "Block " << block_id - 1 << ": " << duration.count() << " ms" << std::endl;
          // }

          // std::ofstream timings_file("../output/block_timings"+std::to_string(grain_size)+".txt");
          // for (const auto &[block_id, duration] : block_timings)
          // {
          //   timings_file << block_id << " " << duration << std::endl;
          // }
        }
        ,tbb::simple_partitioner()
        );

    // Save the time and grain size to the output file etc.
    std::cout << "Time: " << t << " ns" << " Grain size: " << grain_size << std::endl;

    if (file.is_open())
    {
      file << "Time: " << t  << " ns" << " Grain size: " << grain_size << "\n";
    }
    mandelbrot_image.saveToFile("../output/mandelbrot" + std::to_string(grain_size) + ".png");
    std::string str = "../output/time_by_element" + std::to_string(grain_size) + ".png"; //"../output/time_by_element.png"
    color_continious_picture(array, display_width, display_height, str);
  }
  file.close();
  
  return 0;
  }
